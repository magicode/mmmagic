#include <nan.h>
#include <node.h>
#include <node_buffer.h>
#include <string.h>
#include <stdlib.h>

#include "magic.h"

using namespace node;
using namespace v8;

class Magic;

struct Baton {
	uv_work_t request;
	Nan::Callback *callback;

	char* data;
	uint32_t dataLen;
	bool dataIsPath;

	// libmagic info
	const char* path;
	int flags;

	bool error;
	char* error_message;

	const char* result;
	Magic* obj;
};

static Nan::Persistent<v8::Function> constructor;
static const char* fallbackPath;

class Magic: public ObjectWrap {
public:
	const char* mpath;
	struct magic_set *ms;
	int mflags;

	Magic(const char* path, int flags ,bool loadMagic) {
		if (path != NULL) {
			/* Windows blows up trying to look up the path '(null)' returned by
			 magic_getpath() */
			if (strncmp(path, "(null)", 6) == 0)
				path = NULL;
		}
		mpath = (path == NULL ? strdup(fallbackPath) : path);
		mflags = flags;

		ms = NULL;

		if(loadMagic){
			ms = magic_open(mflags | MAGIC_NO_CHECK_COMPRESS | MAGIC_ERROR);
			if (ms == NULL) {

			} else if (magic_load(ms, mpath) == -1
					&& magic_load(ms, fallbackPath) == -1) {
				magic_close(ms);
			}
		}
	}
	~Magic() {
		if (mpath != NULL) {
			free((void*) mpath);
			mpath = NULL;
		}
		if (ms) {
			magic_close(ms);
			ms = NULL;
		}
	}

	static void New(const Nan::FunctionCallbackInfo<v8::Value>& args) {
		Nan::HandleScope scope;

		if (!args.IsConstructCall()) {
		    const int argc = 1; 
		    v8::Local<v8::Value> argv[argc] = {args[0]};
		    v8::Local<v8::Function> cons = Nan::New(constructor);
		    args.GetReturnValue().Set(cons->NewInstance(argc, argv));
		    return;
		}
#ifndef _WIN32
		int mflags = MAGIC_SYMLINK;
#else
		int mflags = MAGIC_NONE;
#endif
		char* path = NULL;
		bool use_bundled = true;

		bool loadMagic = 0;

		

		if (args.Length() > 1) {
			if (args[1]->IsInt32())
				mflags = args[1]->Int32Value();
			if (args[1]->IsBoolean()){
				loadMagic = args[1]->ToBoolean()->Value();
			}
			else {
				return Nan::ThrowError("Second argument must be an integer");
			}
		}

		if(args.Length() > 2 && args[2]->IsBoolean())
			loadMagic = args[2]->Int32Value();

		if (args.Length() > 0) {
			if (args[0]->IsString()) {
				use_bundled = false;
				String::Utf8Value str(args[0]->ToString());
				path = strdup((const char*) (*str));
			} else if (args[0]->IsInt32())
				mflags = args[0]->Int32Value();
			else if (args[0]->IsBoolean() && !args[0]->BooleanValue()) {
				use_bundled = false;
				path = strdup(magic_getpath(NULL, 0/*FILE_LOAD*/));
			} else {
				return Nan::ThrowError("First argument must be a string or integer");
			}
		}



		Magic* obj = new Magic((use_bundled ? NULL : path), mflags,loadMagic);


		obj->Wrap(args.This());
		obj->Ref();
		args.GetReturnValue().Set(args.This());
	}

	static void DetectFile(const Nan::FunctionCallbackInfo<v8::Value>& args) {
		Nan::HandleScope scope;
		
		Magic* obj = ObjectWrap::Unwrap < Magic > (args.This());

		if (!args[0]->IsString()) {
			return Nan::ThrowError("First argument must be a string");
		}
		if (!args[1]->IsFunction()) {
			return Nan::ThrowError("Second argument must be a callback function");
		}
		
		String::Utf8Value str(args[0]->ToString());
		
		Baton* baton = new Baton();
		baton->error = false;
		baton->error_message = NULL;
		baton->request.data = baton;
		baton->callback = new Nan::Callback( args[1].As<v8::Function>() );
		baton->data = strdup((const char*) *str);
		baton->dataIsPath = true;
		baton->path = obj->mpath;
		baton->flags = obj->mflags;
		baton->result = NULL;
		baton->obj = obj;

		int status = uv_queue_work(uv_default_loop(), &baton->request,
				Magic::DetectWork, (uv_after_work_cb) Magic::DetectAfter);
		assert(status == 0);

		args.GetReturnValue().SetUndefined();
	}

	static void Detect(const Nan::FunctionCallbackInfo<v8::Value>& args) {
		Nan::HandleScope scope;
		Magic* obj = ObjectWrap::Unwrap<Magic>(args.This());

		if (args.Length() < 2) {
			return Nan::ThrowError("Expecting 2 arguments");
		}
		if (!Buffer::HasInstance(args[0])) {
			return Nan::ThrowError("First argument must be a Buffer");
		}
		if (!args[1]->IsFunction()) {
			return Nan::ThrowError("Second argument must be a callback function");
		}
		
		Local<Value> buffer_obj = args[0];
		
		Baton* baton = new Baton();
		baton->error = false;
		baton->error_message = NULL;
		baton->request.data = baton;
		baton->callback = new Nan::Callback( args[1].As<v8::Function>() );
		baton->data = Buffer::Data(buffer_obj);
		baton->dataLen = Buffer::Length(buffer_obj);
		baton->dataIsPath = false;
		baton->path = obj->mpath;
		baton->flags = obj->mflags;
		baton->result = NULL;
		baton->obj = obj;
		/*
		const char* result = magic_buffer(obj->ms , (const void*) baton->data , baton->dataLen);

		if (result == NULL) {
			const char* error = magic_error(obj->ms);
			if (error) {
				baton->error = true;
				baton->error_message = strdup(error);
			}
		} else
			baton->result = strdup(result);

		if (baton->error) {
			Local < Value > err = Exception::Error(
					String::New(baton->error_message));
			free(baton->error_message);

			const unsigned argc = 1;
			Local<Value> argv[argc] = {err};

			TryCatch try_catch;
			baton->callback->Call(Context::GetCurrent()->Global(), argc, argv);
			if (try_catch.HasCaught())
				FatalException(try_catch);
		} else {
			const unsigned argc = 2;
			Local<Value> argv[argc] = {
				Local<Value>::New(Null()),
				Local<Value>::New(baton->result
						? String::New(baton->result)
						: String::Empty())
			};

			if (baton->result)
				free((void*) baton->result);

			TryCatch try_catch;
			baton->callback->Call(Context::GetCurrent()->Global(), argc, argv);
			if (try_catch.HasCaught())
				FatalException(try_catch);
		}

		baton->callback.Dispose();
		delete baton;
		*/


		int status = uv_queue_work(uv_default_loop(),
		&baton->request,
		Magic::DetectWork,
		(uv_after_work_cb)Magic::DetectAfter);


		assert(status == 0);
		args.GetReturnValue().SetUndefined();
	}

	static void DetectWork(uv_work_t* req) {
		Baton* baton = static_cast<Baton*>(req->data);

		const char* result;
		struct magic_set *magic;


		if(baton->obj->ms){
			magic = baton->obj->ms;
		}else{

			magic = magic_open(
				baton->flags | MAGIC_NO_CHECK_COMPRESS | MAGIC_ERROR);

			if (magic == NULL) {
			  
			  
#if NODE_MODULE_VERSION <= 0x000B
        baton->error_message = strdup(uv_strerror(uv_last_error(uv_default_loop())));
#else
// XXX libuv 1.x currently has no public cross-platform function to convert an
//     OS-specific error number to a libuv error number. `-errno` should work
//     for *nix, but just passing GetLastError() on Windows will not work ...
# ifdef _MSC_VER
        baton->error_message = strdup(uv_strerror(GetLastError()));
# else
        baton->error_message = strdup(uv_strerror(-errno));
# endif
# endif
			} else if (magic_load(magic, baton->path) == -1
					&& magic_load(magic, fallbackPath) == -1) {
				baton->error_message = strdup(magic_error(magic));
				magic_close(magic);
				magic = NULL;
			}

			if (magic == NULL) {
				if (baton->error_message)
					baton->error = true;
				return;
			}
		}

		if (baton->dataIsPath)
			result = magic_file(magic, baton->data);
		else
			result = magic_buffer(magic, (const void*) baton->data,
					baton->dataLen);

		if (result == NULL) {
			const char* error = magic_error(magic);
			if (error) {
				baton->error = true;
				baton->error_message = strdup(error);
			}
		} else
			baton->result = strdup(result);


		if(!baton->obj->ms)	magic_close(magic);
	}

	static void DetectAfter(uv_work_t* req) {
		Nan::HandleScope scope;
		Baton* baton = static_cast<Baton*>(req->data);

		if (baton->error) {
			Local<Value> err = Exception::Error(Nan::New(baton->error_message).ToLocalChecked());
			free(baton->error_message);

			const unsigned argc = 1;
			Local<Value> argv[argc] = {err};

			Nan::TryCatch try_catch;
			baton->callback->Call(Nan::GetCurrentContext()->Global(), argc, argv);
			if (try_catch.HasCaught())
				Nan::FatalException(try_catch);
		} else {
			const unsigned argc = 2;
			Local<Value> argv[argc] = {
				Nan::Null(),
				(baton->result ? Nan::New(baton->result).ToLocalChecked() : Nan::New("").ToLocalChecked())
			};

			if (baton->result)
				free((void*) baton->result);

			Nan::TryCatch try_catch;
			baton->callback->Call(Nan::GetCurrentContext()->Global(), argc, argv);
			if (try_catch.HasCaught())
				Nan::FatalException(try_catch);
		}

		if (baton->dataIsPath)
			free(baton->data);
		
		delete baton->callback;
		delete baton;
	}

	static void SetFallback(const Nan::FunctionCallbackInfo<v8::Value>& args ) {
		Nan::HandleScope scope;

		if (fallbackPath)
			free((void*) fallbackPath);

		if (args.Length() > 0 && args[0]->IsString()
				&& args[0]->ToString()->Length() > 0) {
			String::Utf8Value str(args[0]->ToString());
			fallbackPath = strdup((const char*) (*str));
		} else
			fallbackPath = NULL;

		args.GetReturnValue().SetUndefined();
	}

	static NAN_MODULE_INIT(Initialize) {
		Nan::HandleScope scope;
		
		v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
		
		Local<String> name = Nan::New("Magic").ToLocalChecked();
		
		tpl->InstanceTemplate()->SetInternalFieldCount(1);
		tpl->SetClassName(name);

		Nan::SetPrototypeMethod(tpl,"detectFile", DetectFile);
		Nan::SetPrototypeMethod(tpl,"detect", Detect);
		Nan::Set(target, Nan::New("setFallback").ToLocalChecked(), Nan::New<FunctionTemplate>(SetFallback)->GetFunction());
		
		Nan::Set(target, name, Nan::GetFunction(tpl).ToLocalChecked());
	}
};


NAN_MODULE_INIT(init) {
	Magic::Initialize(target);
}

NODE_MODULE(magic, init);


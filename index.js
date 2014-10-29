var path = require('path');

var Magic = require('./build/Release/magic');

var fbpath = path.join(__dirname, 'magic', 'magic');
Magic.setFallback(fbpath);


function Magic2(a,b,c){
	
	this._detectList = [];
	this._MagicList = [];
	this._posMagic = 0;
	var cpuno = require('os').cpus().length || 1;

	for(var i =0 ;i< cpuno;i++){
		var magic ;
		if(arguments.length == 1)
			magic = new Magic.Magic(a);
		else if (arguments.length == 2)
			magic = new Magic.Magic(a,b);
		else
			magic = new Magic.Magic(a,b,c);
		this._MagicList.push(magic);
	}
}

Magic2.prototype.detect = function(){
	this._detectList.push(Array.prototype.slice.call(arguments));
	this._nextDetect();
//	var magic = this._MagicList[this._posMagic++%this._MagicList.length];
//	magic.detect.apply(magic, arguments);
}

Magic2.prototype.detectFile = function(){
	var magic = this._MagicList[0];
	magic.detectFile.apply(magic, arguments);
}

Magic2.prototype._nextMagic = function(){
	for(var i in this._MagicList){
		if(this._MagicList[i] && !this._MagicList[i]._duse){
			return this._MagicList[i];
		}
	}
	return;
}

Magic2.prototype._nextDetect = function(){
	var _this = this;

	while(this._detectList.length){
		var magic;
		if( magic = this._nextMagic()){
			var nextArg = this._detectList.shift();
			if(nextArg){
				(function (magic,nextArg){
					magic._duse = true;
					var oldCb = nextArg.pop();
					nextArg.push(function(){
						magic._duse =  false;
						oldCb.apply(this, arguments);
						_this._nextDetect();
					});
					
					magic.detect.apply(magic, nextArg);
				})(magic,nextArg);
			}
		}else{
			break;
		}
	}
}
/*
Magic.Magic.prototype._detect = Magic.Magic.prototype.detect;

Magic.Magic.prototype._nextDetect = function(){
	var _this = this;
	if(!this._inWork && this._detectList.length){
		var nextArg = this._detectList.shift();
		if(nextArg){
			this._inWork = true;
			var oldCb = nextArg.pop();
			nextArg.push(function(){
				_this._inWork =  false;
				oldCb.apply(this, arguments);
				_this._nextDetect();
			});
			this._detect.apply(this, nextArg);
		}else{
			this._nextDetect();
		}
	}
}

Magic.Magic.prototype.detect = function(){
	this._detectList = this._detectList || [];
	this._detectList.push(Array.prototype.slice.call(arguments));
	this._nextDetect();
}
*/
module.exports = {
  Magic: Magic2,
  MAGIC_NONE: 0x000000, /* No flags (default for Windows) */
  MAGIC_DEBUG: 0x000001, /* Turn on debugging */
  MAGIC_SYMLINK: 0x000002, /* Follow symlinks (default for *nix) */
  MAGIC_DEVICES: 0x000008, /* Look at the contents of devices */
  MAGIC_MIME_TYPE: 0x000010, /* Return the MIME type */
  MAGIC_CONTINUE: 0x000020, /* Return all matches */
  MAGIC_CHECK: 0x000040, /* Print warnings to stderr */
  MAGIC_PRESERVE_ATIME: 0x000080, /* Restore access time on exit */
  MAGIC_RAW: 0x000100, /* Don't translate unprintable chars */
  MAGIC_MIME_ENCODING: 0x000400, /* Return the MIME encoding */
  MAGIC_MIME: (0x000010|0x000400), /*(MAGIC_MIME_TYPE|MAGIC_MIME_ENCODING)*/
  MAGIC_APPLE: 0x000800, /* Return the Apple creator and type */

  MAGIC_NO_CHECK_TAR: 0x002000, /* Don't check for tar files */
  MAGIC_NO_CHECK_SOFT: 0x004000, /* Don't check magic entries */
  MAGIC_NO_CHECK_APPTYPE: 0x008000, /* Don't check application type */
  MAGIC_NO_CHECK_ELF: 0x010000, /* Don't check for elf details */
  MAGIC_NO_CHECK_TEXT: 0x020000, /* Don't check for text files */
  MAGIC_NO_CHECK_CDF: 0x040000, /* Don't check for cdf files */
  MAGIC_NO_CHECK_TOKENS: 0x100000, /* Don't check tokens */
  MAGIC_NO_CHECK_ENCODING: 0x200000 /* Don't check text encodings */
};
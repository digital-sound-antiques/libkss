# LIBKSS

LIBKSS is a music player library for MSX music formats, forked from MSXplug.
Supported formats are .kss, .mgs, .bgm, .opx, .mbm.

# How to build

The following step builds `libkss.a` library.

```
$ git clone --recursive https://github.com/digital-sound-antiques/libkss.git
$ cd libkss
$ cmake .
$ make
```

You can also build the KSS to WAV converter binary as follows.

```
$ make kss2wav
```


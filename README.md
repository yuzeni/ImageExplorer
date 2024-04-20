# Image Explorer
This is a basic image viewer, built on [raylib](https://github.com/raysan5/raylib), made for exploring any heap of files and folders for images.\
It may or may not crash on you ;)\
All non-self-explanatory controls are explained upon starting the program. Otherwise, just click around!\
The chronological sorting does not properly work, as EXIF parsing would be required for that kind of information.

<div align="center">
<img src="https://github.com/yuzeni/ImageExplorer/blob/main/misc/demo.jpg" alt="demo" width="800"/>
</div>

## Building on Windows
Run the `build.bat` script from the _"x64 Native Tools Command Prompt"_ or from any command prompt with the vcvars64.bat environment.\
If you want to build [raylib](https://github.com/raysan5/raylib) yourself, make sure to enable all the file formats for image loading in `config.h`.\
Alternatively, a prebuilt windows executable is already located in the build directory.

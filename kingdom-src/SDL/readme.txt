从sdl官网下载sdl源码包后，如何转换为自需要源码包

1、修改SDLmain运行库（Debug和Release不一样设置）
C/C++
  Code Generation      Runtime Library 
           Debug    从Multi-threaded Debug DLL(/MDd)改为Multi-threaded Debug(/MTd)
           Release    从Multi-threaded DLL(/MD)改为Multi-threaded(/MT)

2、SDL：编译完成后把生成文件复制到相应目录（Debug和Release一样设置）
Build Events
  Post-Build Event     
     Command Line填入：
copy $(TargetDir)\$(TargetName).lib ..\..\..\SDL-dev-framework\SDL-1.3.x\lib\
copy $(TargetDir)\$(TargetName).dll ..\..\..\SDL-dev-framework\dll\

3、SDLmain：编译完成后把生成文件复制到相应目录（Debug和Release一样设置）
copy $(TargetDir)\$(TargetName).lib ..\..\..\SDL-dev-framework\SDL-1.3.x\lib\

4、为调试，把相关调试版dll直接复制到kingdom可执行目录下。
Build Events
  Post-Build Event
    Command Line填入： 
copy $(SolutionDir)\..\..\..\SDL\SDL-1.3.0-5538\VisualC\SDL\Debug\SDL.dll $(TargetDir)\SDL.dll
copy $(SolutionDir)\..\..\..\SDL\SDL_ttf-2.0.10\VisualC\Debug\SDL_ttf.dll $(TargetDir)\SDL_ttf.dll
copy $(SolutionDir)\..\..\..\SDL\SDL_mixer-1.2.11\VisualC\Debug\SDL_mixer.dll $(TargetDir)\SDL_mixer.dll

5、SDL_image必须重新编译
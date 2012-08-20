源码包目录分几个部分：

一、kingdom.exe的C/C++源代码

它包括三个目录：

projectfiles：
　　工程项目文件，kingdom.sln/kingdom.vcproj就存储在这里。编程生成的obj和exe也默认放在这个目录。

packaging：
　　kingdom.exe中使用的resource。当前只有一个用于kingodm.exe的icon。

src：kingdom.exe的C/C++源代码。

二、为支持多国语言

po：
　　程序使用gettext机制支持国际语言。里头有*.pot和*.po

translations：
　　由以上po目录生成，最终可执行程序直接使用的*.mo

三、以WML写出的各式各样*.cfg

data：
　　WML脚本文件集合

四、xwml

从data目录形成的，最终可执行程序可直接使用的*.bin文件

五、多媒体资源
fonts：
　　字库文件，*.ttf/ttc

images：
　　图片文件，*.png

sounds：
　　声音文件，*.wav/ogg

六、二进制分发包
redist：
　　当中有kingdom.exe运行时需要*.dll





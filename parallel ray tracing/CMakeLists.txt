# 1. 指定 CMake 的最低版本要求 (这是一个好习惯)
cmake_minimum_required(VERSION 3.10)

# 2. 设置项目名称，这会作为生成解决方案文件的默认名
project(HelloWorldProject)

# 3. 告诉 CMake 我们要创建一个可执行文件
#    - 第一个参数 "HelloApp" 是你想要生成的可执行文件的名字
#    - 第二个参数 "main.cpp" 是编译它所需要的源文件
add_executable(HelloApp main.cpp)
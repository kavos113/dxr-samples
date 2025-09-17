cmake_minimum_required(VERSION 3.28)
project(dxr-sample)

set(CMAKE_CXX_STANDARD 20)

add_executable(dxr-sample
        main.cpp
        Application.cpp
        D3DEngine.cpp
        main.cpp
)
target_link_libraries(dxr-sample PRIVATE d3d12 dxgi d3dcompiler)
target_compile_definitions(dxr-sample PRIVATE DEBUG)

find_package(directxmath CONFIG REQUIRED)
target_link_libraries(dxr-sample PRIVATE Microsoft::DirectXMath)

find_package(directx-dxc CONFIG REQUIRED)
target_link_libraries(dxr-sample PRIVATE Microsoft::DirectXShaderCompiler)

file(COPY shader.hlsl DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
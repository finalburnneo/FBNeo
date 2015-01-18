#pragma once

#include <fstream>
#include <string>
#include <sstream>
#include <iostream>

#include <GL/glew.h>

using namespace std;

class Shader
{
    void buildProgram(const string &vsrc, const string &fsrc);
    bool compileShader(GLuint shader, const string& source);
public:
    GLuint program;
    Shader(const string& vertex, const string& fragment, bool isSource=false);
    ~Shader();
    void use();
    void setUniform(const string& name, float v);
    void setUniform(const string& name, GLint v);
};

#include <exception>
#include "shader.h"

void Shader::buildProgram(const string& vsrc, const string& fsrc)
{
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    program = glCreateProgram();

    bool vsLoaded = compileShader(vs, vsrc);
    bool fsLoaded = compileShader(fs, fsrc);

    if (vsLoaded || fsLoaded) {
        if (vsLoaded)
            glAttachShader(program, vs);
        if (fsLoaded)
            glAttachShader(program, fs);
        glLinkProgram(program);

        GLint result;
        GLchar log[512];
        glGetProgramiv(program, GL_LINK_STATUS, &result);

        if (!result) {
            glGetProgramInfoLog(program, sizeof(log), nullptr, log);
            cerr << "Erro ao ligar o programa: " << log << endl;
        }
    }

    // Limpa o que não é mais necessário
    glDeleteShader(vs);
    glDeleteShader(fs);
}

bool Shader::compileShader(GLuint shader, const string &source)
{
    if (source.empty()) {
        cerr  << "Shader erro: código fonte vazio!" << endl;
        return false;
    }
    const char *ptr = source.c_str();
    glShaderSource(shader, 1, &ptr, nullptr);
    glCompileShader(shader);

    GLint result;
    GLchar log[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &result);

    if (result)
        return true;

    glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
    cerr << "Erro ao carregar o shader: " << log << endl;

    return false;
}

Shader::Shader(const string &vertex, const string &fragment, bool isSource)
{
    if (isSource) {
        buildProgram(vertex, fragment);
    } else {
        string vertexSrc;
        string fragmentSrc;
        try {
            ifstream vsFile(vertex);
            ifstream fsFile(fragment);

            stringstream vss, fss;
            vss << vsFile.rdbuf();
            fss << fsFile.rdbuf();

            vsFile.close();
            fsFile.close();

            vertexSrc = vss.str();
            fragmentSrc = fss.str();
        } catch (exception e) {
            cerr << "Falha ao carregar shader" << endl;
        }
        cout << "Shader carregado com sucesso!" << endl;
        buildProgram(vertexSrc, fragmentSrc);
    }
}

Shader::~Shader()
{
    glDeleteProgram(program);
}

void Shader::use()
{
    glUseProgram(program);
}

void Shader::setUniform(const string &name, float v)
{
    GLuint loc = glGetUniformLocation(program, name.c_str());
    glUniform1f(loc, v);
}

void Shader::setUniform(const string &name, GLint v)
{
    GLuint loc = glGetUniformLocation(program, name.c_str());
    glUniform1i(loc, v);
}

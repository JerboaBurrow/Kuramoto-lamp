#ifndef MAIN_H
#define MAIN_H

#include <jGL/jGL.h>
#include <jGL/OpenGL/openGLInstance.h>
#include <jGL/OpenGL/Shader/glShader.h>
#include <jGL/shape.h>

#include <logo.h>
#include <jGL/Display/desktopDisplay.h>
#include <jGL/orthoCam.h>

#include <jLog/jLog.h>

#include <rand.h>
#include <algorithm>
#include <chrono>
#include <sstream>

#include <glCompute.h>

using namespace std::chrono;

// to also test clipping
int resX = 1920;
int resY = 1080;

int cells = 1024;
int shells = 16;
float k = 10.0;
float o = 0.1;
float kd = 1.0;
float eta = 0.0;
float kp = 1.0;

std::vector<float> coef = {1.0};
std::vector<float> shifts = {0.0};

uint8_t frameId = 0;
double deltas[60];

bool debug = true;
bool paused = false;

std::unique_ptr<jGL::jGLInstance> jGLInstance;

std::string fixedLengthNumber(double x, unsigned length)
{
    std::string d = std::to_string(x);
    std::string dtrunc(length,' ');
    for (unsigned c = 0; c < dtrunc.length(); c++/*ayy lmao*/)
    {

        if (c >= d.length())
        {
            dtrunc[c] = '0';
        }
        else
        {
            dtrunc[c] = d[c];
        }
    }
    return dtrunc;
}

struct Visualise
{
    Visualise(GLuint texture)
    : texture(texture)
    {
        shader = jGL::GL::glShader(vertexShader, fragmentShader);
        shader.compile();
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData
        (
            GL_ARRAY_BUFFER,
            sizeof(float)*6*4,
            &quad[0],
            GL_STATIC_DRAW
        );
        glEnableVertexAttribArray(0);
        glVertexAttribPointer
        (
            0,
            4,
            GL_FLOAT,
            false,
            4*sizeof(float),
            0
        );
        glVertexAttribDivisor(0,0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void draw()
    {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, texture);
        shader.setUniform("tex", jGL::Sampler2D(1));

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }

    jGL::GL::glShader shader;
    GLuint texture, vao, vbo;
    float quad[6*4] =
    {
        -1.0, -1.0, 0.0, 0.0,
         1.0, -1.0, 1.0, 0.0,
         1.0,  1.0, 1.0, 1.0,
        -1.0, -1.0, 0.0, 0.0,
        -1.0,  1.0, 0.0, 1.0,
         1.0,  1.0, 1.0, 1.0
    };
    const char * vertexShader =
    "#version " GLSL_VERSION "\n"
    "precision highp float;\n"
    "precision highp int;\n"
    "layout(location = 0) in vec4 a_position;\n"
    "out vec2 o_texCoords;\n"
    "void main(){\n"
    "   gl_Position = vec4(a_position.xy,0.0,1.0);\n"
    "   o_texCoords = a_position.zw;\n"
    "}";

    const char * fragmentShader =
    "#version " GLSL_VERSION "\n"
    "precision highp float;\n"
    "precision highp int;\n"
    "layout(location = 0) out vec4 frag;\n"
    "in vec2 o_texCoords;\n"
    "uniform highp sampler2D tex;\n"
    "float poly(float x, float p0, float p1, float p2, float p3, float p4){\n"
    "    float x2 = x*x; float x4 = x2*x2; float x3 = x2*x;\n"
    "    return clamp(p0+p1*x+p2*x2+p3*x3+p4*x4,0.0,1.0);\n"
    "}\n"
    "vec3 cmap(float t)\n{"
    "    return vec3( poly(t,0.91, 3.74, -32.33, 57.57, -28.99), poly(t,0.2, 5.6, -18.89, 25.55, -12.25), poly(t,0.22, -4.89, 22.31, -23.58, 5.97) );\n"
    "}\n"
    "void main(){\n"
    "    float theta = mod(texture(tex, o_texCoords).r, 2.0*3.14159);\n"
    "    if (theta < 0) { theta += 2.0*3.14159;}\n"
    "    frag = vec4(cmap(theta/(2.0*3.14159)), 1.0);\n"
    "}";
};

struct Kuramoto
{
    std::vector<std::vector<std::pair<int, float>>> K;
    std::vector<float> expansion_coefficients = {1.0};
    std::vector<float> shifts = {0.0};

    std::vector<float> interaction(std::vector<float> & theta, std::vector<float> & dtheta)
    {
        for (int i = 0; i < K.size(); i++)
        {
            for (auto & jk : K[i])
            {
                dtheta[i] += jk.second*kernel(theta[jk.first]-theta[i]);
            }
        }

        return dtheta;
    }

    float kernel(float phi)
    {
        float k = 0.0;
        for (int i = 0; i < expansion_coefficients.size(); i++)
        {
            k += expansion_coefficients[i]*std::sin((i+1)*phi+shifts[i]);
        }
        return k;
    }
};

const char * kuramotoComputeShader =
    "#version " GLSL_VERSION "\n"
    "precision highp float;\n"
    "precision highp int;\n"
    "in vec2 o_texCoords;\n"
    "layout(location=0) out float output;\n"
    "uniform highp sampler2D theta;\n"
    "uniform highp sampler2D noise;\n"
    "uniform int n;\n"
    "uniform int s;\n"
    "uniform float k;\n"
    "uniform float kp;\n"
    "uniform float dt;\n"
    "uniform float D;\n"
    "uniform float kd;\n"
    "uniform float o;\n"
    "uniform int coreShell;\n"
    "uniform vec4 coef; uniform vec4 shift;\n"
    "float random(vec2 st){\n"
    "    return clamp(fract(sin(dot(st.xy, vec2(12.9898,78.233))) * 43758.5453123), 0.001, 1.0);\n"
    "}\n"
    "float kernel(float phi, vec4 coef, vec4 shift){\n"
    "    return coef.r*sin(phi+shift.r) + \n"
    "           coef.g*sin(2.0*phi+shift.g)+\n"
    "           coef.b*sin(3.0*phi+shift.b)+\n"
    "           coef.a*sin(4.0*phi+shift.a);\n"
    "}\n"
    "float distance(int i1, int j1, int i2, int j2, int l){\n"
    "    float rx = float(i1-i2); float ry = float(j1-j2);\n"
    "    if (rx < 0.5*float(l)) { rx += float(l); }\n"
    "    if (rx >= 0.5*float(l)) { rx -= float(l); }\n"
    "    if (ry < 0.5*float(l)) { ry += float(l); }\n"
    "    if (ry >= 0.5*float(l)) { ry -= float(l); }\n"
    "    return rx*rx+ry*ry;\n"
    "}\n"
    "void main(){\n"
    "    int i = int(o_texCoords.x*float(n)); int j = int(o_texCoords.y*float(n));\n"
    "    float ijtheta = texture(theta, o_texCoords).r;\n"
    "    float dtheta = 0.0;\n"
    "    float r = texture(noise, o_texCoords).r;\n"
    "    float count = 1.0;"
    "    vec2 seedK = vec2(r, r*float(i)/float(1.0+j));\n"
    "    for (int ix = -s; ix <= s; ix++){\n"
    "        for(int iy = -s; iy <= s; iy++){\n"
    "            int ni = int(mod(ix+i,n)); int nj = int(mod(iy+j,n));\n"
    "            ni = clamp(ni, 0, n); nj = clamp(nj, 0, n);\n"
    "            vec2 seedKp = vec2(r*float(ni)/float(1.0+nj), r*float(i)/float(1.0+j));\n"
    "            float d = distance(i, j, ni, nj, n);\n"
    "            bool firstShell = ix >= -coreShell && ix <= coreShell && iy >= -coreShell && iy <= coreShell;"
    "            if (firstShell){\n"
    "                vec2 ntexCoords = vec2(float(ni)/float(n), float(nj)/float(n));\n"
    "                float phi = texture(theta, ntexCoords).r-ijtheta;\n"
    "                dtheta += k*kernel(phi, coef, shift);\n"
    "                count += 1.0;"
    "            }\n"
    "            else if (random(seedKp) < kp*(1/(kd*d))){\n"
    "                vec2 ntexCoords = vec2(float(ni)/float(n), float(nj)/float(n));\n"
    "                float phi = texture(theta, ntexCoords).r-ijtheta;\n"
    "                dtheta += k*random(seedK)*kernel(phi, coef, shift);\n"
    "                count += 1.0;"
    "            }\n"
    "        }\n"
    "    }\n"
    "    float omega = o*random(vec2(float(i), float(j)));\n"
    "    float diff = random(vec2(float(i)*float(j)+ijtheta, ijtheta));\n"
    "    output = ijtheta+dt*(omega+D*diff+dtheta/count);\n"
    "}";

float clamp(float x, float low, float high)
{
    return std::min(std::max(x, low), high);
}

float poly(float x, float p0, float p1, float p2, float p3, float p4)
{
   float x2 = x*x; float x4 = x2*x2; float x3 = x2*x;
   return clamp(p0+p1*x+p2*x2+p3*x3+p4*x4,0.0,1.0);
}

glm::vec3 cmap(float t)
{
    return glm::vec3( poly(t,0.91, 3.74, -32.33, 57.57, -28.99), poly(t,0.2, 5.6, -18.89, 25.55, -12.25), poly(t,0.22, -4.89, 22.31, -23.58, 5.97) );
}

std::vector<std::pair<int, int>> shell(int i, int j, int s, int l)
{
    std::vector<std::pair<int, int>> indices;
    for (int n = -s; n <= s; n++)
    {
        for (int m = -s; m <= s; m++)
        {
            int ix = n+i % l;
            int iy = m+j % l;
            if (ix < 0) { ix += l; }
            if (iy < 0) { iy += l; }
            indices.push_back({ix, iy});
        }
    }
    return indices;
}

#endif /* MAIN_H */

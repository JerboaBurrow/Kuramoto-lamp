#ifndef GLCOMPUTE_H
#define GLCOMPUTE_H

#include <jGL/OpenGL/gl.h>
#include <jGL/OpenGL/Shader/glShader.h>

#include <vector>
#include <map>
#include <string>
#include <algorithm>
#include <stdexcept>

void initTexture2DR32F(GLuint id, uint64_t n, uint64_t m)
{
    glBindTexture(GL_TEXTURE_2D,id);
    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MIN_FILTER,
        GL_NEAREST
    );
    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MAG_FILTER,
        GL_NEAREST
    );
    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_S,
        GL_CLAMP_TO_EDGE
    );
    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_T,
        GL_CLAMP_TO_EDGE
    );
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_R32F,
        n,
        m,
        0,
        GL_RED,
        GL_FLOAT,
        NULL
    );
}

void transferToTexture2DR32F(GLuint id, std::vector<float> data, uint64_t n, uint64_t m)
{
    glBindTexture(GL_TEXTURE_2D,id);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_R32F,
        n,
        m,
        0,
        GL_RED,
        GL_FLOAT,
        data.data()
    );
}

class glCompute
{

public:

    jGL::GL::glShader shader;

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

    glCompute
    (
        std::map<std::string, std::pair<uint64_t, uint64_t>> attributeSize,
        std::pair<uint64_t, uint64_t> outputSize,
        const char * fragmentShader
    )
    : outputSize(outputSize)
    {
        shader = jGL::GL::glShader(vertexShader, fragmentShader);
        shader.compile();
        copyShader.compile();
        uint64_t n = attributeSize.size();
        textures.resize(n+1);
        glGenTextures(n+1, textures.data());
        uint64_t t = 0;
        for (auto & attr : attributeSize)
        {
            attributes[attr.first] = Attribute
            (
                std::vector<float>(attr.second.first*attr.second.second, 0.0),
                textures[t],
                attr.second.first,
                attr.second.second
            );
            initTexture2DR32F(textures[t], attr.second.first, attr.second.second);
            t++;
        }
        initTexture2DR32F(textures.back(), outputSize.first, outputSize.second);
        output = std::vector<float>(outputSize.first*outputSize.second, 0.0);
        transferToTexture2DR32F(textures.back(), output, outputSize.first, outputSize.second);
        glGenFramebuffers(1, &frameBuffer);
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

    ~glCompute()
    {
        glDeleteTextures(attributes.size(), textures.data());
        glDeleteFramebuffers(1, &frameBuffer);
        glDeleteBuffers(1, &vbo);
        glDeleteVertexArrays(1, &vao);
    }

    void set(std::string attribute, std::vector<float> newData)
    {
        if (attributes.find(attribute) != attributes.end())
        {
            if (newData.size() <= attributes[attribute].data.size())
            {
                std::copy
                (
                    newData.begin(),
                    newData.end(),
                    attributes[attribute].data.begin()
                );
            }
        }
    }

    void set(std::string attribute, float datum, uint64_t index)
    {
        if (attributes.find(attribute) != attributes.end())
        {
            if (index <= attributes[attribute].data.size())
            {
                attributes[attribute].data[index] = datum;
            }
        }
    }

    std::vector<float> & get(std::string attribute)
    {
        if (attributes.find(attribute) != attributes.end())
        {
            return attributes[attribute].data;
        }
        throw std::runtime_error("No attribute: "+attribute);
    }

    GLuint getTexture(std::string attribute)
    {
        if (attributes.find(attribute) != attributes.end())
        {
            return attributes[attribute].texture;
        }
        throw std::runtime_error("No attribute: "+attribute);
    }

    const std::vector<float> & result() { return output; }

    void sync(std::string attribute)
    {
        if (attributes.find(attribute) != attributes.end())
        {
            Attribute & attr = attributes[attribute];
            transferToTexture2DR32F(attr.texture, attr.data, attr.dimX, attr.dimY);
        }
    }

    void sync()
    {
        for (const auto & attr : attributes)
        {
            sync(attr.first);
        }
    }

    GLuint outputTexture() { return textures.back(); }

    void glCopyTexture(GLuint from, GLuint to, glm::vec2 size)
    {
        copyShader.use();
        glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, to);

        glFramebufferTexture2D
        (
            GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D,
            to,
            0
        );
        GLenum drawBuffers[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, drawBuffers);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, from);
        copyShader.setUniform("from", jGL::Sampler2D(2));

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);

        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);

        glDepthMask(false);
        glDisable(GL_BLEND);
        glViewport(0, 0, size.x, size.y);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindVertexArray(0);
    }

    void compute(bool syncResult)
    {
        shader.use();

        glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, textures.back());

        glFramebufferTexture2D
        (
            GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D,
            textures.back(),
            0
        );
        GLenum drawBuffers[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, drawBuffers);

        GLuint t = 0;
        for (const auto & attr : attributes)
        {
            glActiveTexture(GL_TEXTURE0+t+2);
            glBindTexture(GL_TEXTURE_2D, attr.second.texture);
            shader.setUniform(attr.first, jGL::Sampler2D(t+2));
            t++;
        }

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);

        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);

        glDepthMask(false);
        glDisable(GL_BLEND);
        glViewport(0, 0, outputSize.first, outputSize.second);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        if (syncResult)
        {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, textures.back());
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, output.data());
        }

        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindVertexArray(0);
    }

    const std::vector<float> & syncResult()
    {
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels
        (
            0,
            0,
            outputSize.first,
            outputSize.second,
            GL_RED,
            GL_FLOAT,
            output.data()
        );
        return result();
    }

private:

    struct Attribute
    {
        Attribute() = default;

        Attribute
        (
            std::vector<float> data,
            GLuint texture,
            uint64_t dimX,
            uint64_t dimY
        )
        : data(data), texture(texture), dimX(dimX), dimY(dimY)
        {}

        std::vector<float> data;
        GLuint texture;
        uint64_t dimX;
        uint64_t dimY;
    };

    std::vector<GLuint> textures;
    std::map<std::string, Attribute> attributes;

    std::vector<float> output;
    std::pair<uint64_t, uint64_t> outputSize;
    GLuint frameBuffer, vao, vbo;

    float quad[6*4] =
    {
        -1.0, -1.0, 0.0, 0.0,
         1.0, -1.0, 1.0, 0.0,
         1.0,  1.0, 1.0, 1.0,
        -1.0, -1.0, 0.0, 0.0,
        -1.0,  1.0, 0.0, 1.0,
         1.0,  1.0, 1.0, 1.0
    };

    const char * copyFragmentShader =
        "#version " GLSL_VERSION "\n"
        "precision highp float;\n"
        "precision highp int;\n"
        "layout(location = 0) out vec4 frag;\n"
        "in vec2 o_texCoords;\n"
        "uniform sampler2D from;\n"
        "void main(){\n"
        "   frag = texture(from, o_texCoords);\n"
        "}";
    jGL::GL::glShader copyShader = jGL::GL::glShader(vertexShader, copyFragmentShader);
};

#endif /* GLCOMPUTE_H */

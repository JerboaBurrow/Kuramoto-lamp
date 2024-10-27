#include "main.h"

int main(int argv, char ** argc)
{

    if (argv >= 3)
    {
        std::map<std::string, std::string> args;
        std::vector<std::string> inputs;
        for (int i = 1; i < argv; i++)
        {
            inputs.push_back(argc[i]);
        }
        std::reverse(inputs.begin(), inputs.end());
        while (inputs.size() >= 2)
        {
            std::string arg = inputs.back();
            inputs.pop_back();
            args[arg] = inputs.back();
            inputs.pop_back();
        }

        if (args.find("-shells") != args.end())
        {
            shells = std::stoi(args["-shells"]);
        }

        if (args.find("-cells") != args.end())
        {
            cells = std::stoi(args["-cells"]);
        }

        if (args.find("-k") != args.end())
        {
            k = std::stof(args["-k"]);
        }

        if (args.find("-kd") != args.end())
        {
            kd = std::stof(args["-kd"]);
        }

        if (args.find("-o") != args.end())
        {
            o = std::stof(args["-o"]);
        }

        if (args.find("-eta") != args.end())
        {
            eta = std::stof(args["-eta"]);
        }

        if (args.find("-kp") != args.end())
        {
            kp = std::stof(args["-kp"]);
        }

        if (args.find("-coefs") != args.end())
        {
            std::stringstream c(args["-coefs"]);
            float f;
            coef.clear();
            while (c >> f)
            {
                coef.push_back(f);
                std::cout << f << "\n";
            }
        }

        if (args.find("-shifts") != args.end())
        {
            std::stringstream c(args["-shifts"]);
            float f;
            shifts.clear();
            while (c >> f)
            {
                shifts.push_back(f);
            }
        }
    }

    while (coef.size() < shifts.size())
    {
        coef.push_back(1.0);
    }

    while (shifts.size() < coef.size())
    {
        shifts.push_back(0.0);
    }

    jGL::DesktopDisplay::Config conf;

    conf.VULKAN = false;

    #ifdef MACOS
    conf.COCOA_RETINA = true;
    #endif
    jGL::DesktopDisplay display(glm::ivec2(resX, resY), "Shape", conf);
    display.setFrameLimit(60);

    glewInit();

    glm::ivec2 res = display.frameBufferSize();
    resX = res.x;
    resY = res.y;

    jGLInstance = std::move(std::make_unique<jGL::GL::OpenGLInstance>(res));

    jGL::OrthoCam camera(resX, resY, glm::vec2(0.0,0.0));

    camera.setPosition(0.0f, 0.0f);

    jLog::Log log;

    high_resolution_clock::time_point tic, tock;
    double rdt = 0.0;

    jGLInstance->setTextProjection(glm::ortho(0.0,double(resX),0.0,double(resY)));
    jGLInstance->setMSAA(1);

    std::vector<jGL::Shape> shapes;
    std::vector<jGL::Transform> trans;
    std::vector<glm::vec4> cols;

    RNG rng;
    int n = cells*cells;

    std::shared_ptr<jGL::ShapeRenderer> rects = jGLInstance->createShapeRenderer
    (
        n
    );

    shapes.reserve(n);
    trans.reserve(n);
    cols.reserve(n);

    float scale = camera.screenToWorld(float(resX)/float(cells), 0.0f).x;

    for (unsigned i = 0; i < n; i++)
    {
        trans.push_back(jGL::Transform((i%cells)/float(cells)+scale/2.0f, (std::floor(i/float(cells)))/float(cells)+scale/2.0f, 0.0, scale));
        cols.push_back(glm::vec4(rng.nextFloat(), rng.nextFloat(), rng.nextFloat(), 1.0));
        shapes.push_back
        (
            {
                &trans[i],
                &cols[i]
            }
        );

        rects->add(shapes[i], std::to_string(i));
    }

    Kuramoto model;
    model.expansion_coefficients = coef;
    model.shifts = shifts;
    model.K.resize(n);
    std::vector<float> omega(n, 0.0);
    std::vector<float> theta(n, 0.0);
    std::vector<float> dtheta(n, 0.0);
    std::vector<int> counts(n, 0);
    float dcrit2 = 0.05*0.05;
    float l = 1.0;

    for (int i = 0; i < n; i++)
    {
        omega[i] = rng.nextFloat()*o;
        theta[i] = rng.nextFloat()*2.0*3.14159;

        auto s = shell(i%cells, std::floor(i/float(cells)), shells, cells);
        for (auto & ij : s)
        {
            int rx = i%cells-ij.first; int ry = std::floor(i/float(cells))-ij.second;
            if (rx < 0.5*cells) { rx += cells; }
            if (rx >= 0.5*cells) { rx -= cells; }
            if (ry < 0.5*cells) { ry += cells; }
            if (ry >= 0.5*cells) { ry -= cells; }
            float d = float(rx*rx+ry*ry);
            if (rng.nextFloat() < kp*(1/(kd*d)))
            {
                auto n = ij.first*cells + ij.second;
                model.K[i].push_back({n, k*rng.nextFloat()});
                counts[i] += 1;
            }
        }
        counts[i] += 1;
    }
    std::shared_ptr<jGL::Shader> shader = std::make_shared<jGL::GL::glShader>
    (
        jGL::GL::glShapeRenderer::shapeVertexShader,
        jGL::GL::glShapeRenderer::rectangleFragmentShader
    );

    shader->use();

    double delta = 0.0;
    double dt = 1.0/60.0;
    float D = std::sqrt(2.0*eta*1.0/dt);
    jGL::ShapeRenderer::UpdateInfo uinfo;

    while (display.isOpen())
    {
        tic = high_resolution_clock::now();

        if (display.keyHasEvent(GLFW_KEY_DOWN, jGL::EventType::PRESS))
        {
            camera.incrementZoom(-1.0f);
        }
        if (display.keyHasEvent(GLFW_KEY_UP, jGL::EventType::PRESS))
        {
            camera.incrementZoom(1.0f);
        }

        if (display.keyHasEvent(GLFW_KEY_SPACE, jGL::EventType::PRESS))
        {
            paused = !paused;
        }

        jGLInstance->beginFrame();

            jGLInstance->clear();

            if (!paused)
            {
                model.interaction(theta, dtheta);
                for (int i = 0; i < n; i++)
                {
                    auto & col = cols[i];
                    theta[i] += dt * (omega[i] + rng.nextNormal()*D + (1.0/float(counts[i]))*dtheta[i]);
                    theta[i] = fmod(theta[i], 2.0*3.14159);
                    if (theta[i] < 0)
                    {
                        theta[i] += 2.0*3.14159;
                    }
                    glm::vec3 newColour = cmap(fmod(theta[i], 2.0*3.14159)/(2.0*3.14159));
                    col.r = newColour.r;
                    col.g = newColour.g;
                    col.b = newColour.b;
                    dtheta[i] = 0.0;
                }
            }

            rects->draw(shader, uinfo);
            rects->setProjection(camera.getVP());

            delta = 0.0;
            for (int n = 0; n < 60; n++)
            {
                delta += deltas[n];
            }
            delta /= 60.0;

            std::stringstream debugText;

            double mouseX, mouseY;
            display.mousePosition(mouseX,mouseY);

            debugText << "Delta: " << fixedLengthNumber(delta,6)
                    << " ( FPS: " << fixedLengthNumber(1.0/delta,4)
                    << ")\n"
                    << "Render draw time: \n"
                    << "   " << fixedLengthNumber(rdt, 6) << "\n"
                    << "Mouse (" << fixedLengthNumber(mouseX,4)
                    << ","
                    << fixedLengthNumber(mouseY,4)
                    << ")\n";

            if (debug)
            {
                jGLInstance->text(
                    debugText.str(),
                    glm::vec2(64.0f, resY-64.0f),
                    0.5f,
                    glm::vec4(0.0f,0.0f,0.0f,1.0f)
                );
            }

            if (frameId == 30)
            {
                if (log.size() > 0)
                {
                    std::cout << log << "\n";
                }
            }

        jGLInstance->endFrame();

        display.loop();

        tock = high_resolution_clock::now();

        deltas[frameId] = duration_cast<duration<double>>(tock-tic).count();
        frameId = (frameId+1) % 60;
        uinfo.scale = false;

    }

    jGLInstance->finish();

    return 0;
}
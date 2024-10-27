#include <main.h>

int main(int argv, char ** argc)
{

    int durationSeconds = 10;

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

        if (args.find("-durationSeconds") != args.end())
        {
            durationSeconds = std::stoi(args["-durationSeconds"]);
        }
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
    std::vector<float> omega(n, 0.0);
    std::vector<float> theta(n, 0.0);
    std::vector<float> dtheta(n, 0.0);
    std::vector<int> counts(n, 0);

    for (int i = 0; i < n; i++)
    {
        theta[i] = rng.nextFloat()*2.0*3.14159;
    }

    std::shared_ptr<jGL::Shader> shader = std::make_shared<jGL::GL::glShader>
    (
        jGL::GL::glShapeRenderer::shapeVertexShader,
        jGL::GL::glShapeRenderer::rectangleFragmentShader
    );

    shader->use();

    double delta = 0.0;
    double dt = 1.0/300.0;
    float D = std::sqrt(2.0*eta*1.0/dt);
    jGL::ShapeRenderer::UpdateInfo uinfo;

    std::vector<float> noise(cells*cells, 0.0);
    for (int i = 0; i < noise.size(); i++)
    {
        noise[i] = rng.nextFloat();
    }

    glCompute compute
    (
        {
            {"theta", {cells, cells}},
            {"noise", {cells, cells}}
        },
        {cells, cells},
        kuramotoComputeShader
    );

    compute.shader.setUniform("n", cells);
    compute.shader.setUniform("s", shells);
    compute.shader.setUniform("coef", glm::vec4(1,0,0,0));
    compute.shader.setUniform("shift", glm::vec4(0,0,0,0));
    compute.shader.setUniform("k", k);
    compute.shader.setUniform("kp", kp);
    compute.shader.setUniform("D", float(std::sqrt(2.0*eta/dt)));
    compute.shader.setUniform("dt", float(dt));
    compute.shader.setUniform("kd", kd);
    compute.shader.setUniform("o", o);
    compute.shader.setUniform("coreShell", 5);
    compute.set("noise", noise);
    compute.set("theta", theta);
    compute.sync();

    Visualise vis(compute.outputTexture());

    auto start = std::chrono::steady_clock::now();

    while (display.isOpen())
    {
        tic = high_resolution_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()-start).count() > durationSeconds)
        {
            start = std::chrono::steady_clock::now();
            shells = int(rng.nextFloat()*10)+5;
            k = rng.nextFloat()*50.0+1;
            kd = rng.nextFloat()*0.01;
            o = rng.nextFloat()*10.0;
            eta = rng.nextFloat()*(0.01*k);
            kp = rng.nextFloat()+0.25;

            std::cout << "shells: " << shells << " "
                        << "k: " << k << " "
                        << "kd: " << kd << " "
                        << "o: " << o << " "
                        << "eta: " << eta << " "
                        << "kp: " << kp << "\n";

            glm::vec4 coef = {1.0, 0.0, 0.0, 0.0};
            glm::vec4 shifts = {0.0, 0.0, 0.0, 0.0};
            for (int i = 0; i < int(rng.nextFloat()*4); i++)
            {
                coef[i] = rng.nextFloat();
                shifts[i] = rng.nextFloat();
            }

            for (int i = 0; i < 4; i++)
            {
                std::cout << coef[i] << " ";
            }
            std::cout << "\n";
            for (int i = 0; i < 4; i++)
            {
                std::cout << shifts[i] << " ";
            }
            std::cout << "\n\n";

            compute.shader.setUniform("n", cells);
            compute.shader.setUniform("s", shells);
            compute.shader.setUniform("coef", coef);
            compute.shader.setUniform("shift", shifts);
            compute.shader.setUniform("k", k);
            compute.shader.setUniform("kp", kp);
            compute.shader.setUniform("kd", kd);
            compute.shader.setUniform("D", float(std::sqrt(2.0*eta/dt)));
            compute.shader.setUniform("dt", float(dt));
            compute.shader.setUniform("o", o);

        }

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

        compute.compute(false);
        compute.glCopyTexture
        (
            compute.outputTexture(),
            compute.getTexture("theta"),
            glm::vec2(cells, cells)
        );

        glClearColor(1.0,1.0,1.0,1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        vis.draw();

        delta = 0.0;
        for (int n = 0; n < 60; n++)
        {
            delta += deltas[n];
        }
        delta /= 60.0;

        if (frameId == 59)
        {
            std::cout << "FPS: " << fixedLengthNumber(1.0/delta,4) << "\n";
        }

        display.loop();

        tock = high_resolution_clock::now();

        deltas[frameId] = duration_cast<duration<double>>(tock-tic).count();
        frameId = (frameId+1) % 60;
        uinfo.scale = false;

    }

    jGLInstance->finish();

    return 0;
}
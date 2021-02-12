#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <string>
#include <utility>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <windows.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

std::list<std::pair<double, double>> euler(std::function<double(double, double)> const& func, unsigned iterations, double y0, double t0 = 0.0, double stepSize = 0.5)
{
	std::list<std::pair<double, double>> result;

	double y = y0, t = t0;

	result.emplace_back(std::make_pair(t, y));

	for (unsigned n = 1; n <= iterations; ++n)
	{
		y += stepSize * func(t, y);
		t += stepSize;

		result.emplace_back(std::make_pair(t, y));
	}

	return result;
}

std::list<std::tuple<double, double, double>> euler(std::function<double(double, double, double)> const& func1, std::function<double(double, double, double)> const& func2, unsigned iterations, double x0, double y0, double t0 = 0.0, double stepSize = 0.5)
{
	std::list<std::tuple<double, double, double>> result;

	double x = x0, y = y0, t = t0;

	result.emplace_back(std::make_tuple(t, x, y));

	for (unsigned n = 1; n <= iterations; ++n)
	{
		x += stepSize * func1(t, x, y);
		y += stepSize * func2(t, x, y);
		t += stepSize;

		result.emplace_back(std::make_tuple(t, x, y));
	}

	return result;
}

std::list<std::pair<double, double>> midpoint(std::function<double(double, double)> const& func, unsigned iterations, double y0, double t0 = 0.0, double stepSize = 0.5)
{
	std::list<std::pair<double, double>> result;

	double y = y0, t = t0;

	result.emplace_back(std::make_pair(t, y));

	for (unsigned n = 1; n <= iterations; ++n)
	{
		double K1 = func(t, y);
		double K2 = func(t + stepSize * 0.5, y + stepSize * 0.5 * K1);
		y += stepSize * K2;
		t += stepSize;

		result.emplace_back(std::make_pair(t, y));
	}

	return result;
}

std::list<std::tuple<double, double, double>> midpoint(std::function<double(double, double, double)> const& func1, std::function<double(double, double, double)> const& func2, unsigned iterations, double x0, double y0, double t0 = 0.0, double stepSize = 0.5)
{
	std::list<std::tuple<double, double, double>> result;

	double x = x0, y = y0, t = t0;

	result.emplace_back(std::make_tuple(t, x, y));

	for (unsigned n = 1; n <= iterations; ++n)
	{
		double K1x = func1(t, x, y);
		double K1y = func2(t, x, y);
		double K2x = func1(t + stepSize * 0.5, x + stepSize * 0.5 * K1x, y + stepSize * 0.5 * K1y);
		double K2y = func2(t + stepSize * 0.5, x + stepSize * 0.5 * K1x, y + stepSize * 0.5 * K1y);
		x += stepSize * K2x;
		y += stepSize * K2y;
		t += stepSize;

		result.emplace_back(std::make_tuple(t, x, y));
	}

	return result;
}

std::list<std::pair<double, double>> modifiedEuler(std::function<double(double, double)> const& func, unsigned iterations, double y0, double t0 = 0.0, double stepSize = 0.5)
{
	std::list<std::pair<double, double>> result;

	double y = y0, t = t0;

	result.emplace_back(std::make_pair(t, y));

	for (unsigned n = 1; n <= iterations; ++n)
	{
		double K1 = func(t, y);
		double K2 = func(t + stepSize, y + stepSize * K1);
		y += stepSize * 0.5 * (K1 + K2);
		t += stepSize;

		result.emplace_back(std::make_pair(t, y));
	}

	return result;
}

std::list<std::tuple<double, double, double>> modifiedEuler(std::function<double(double, double, double)> const& func1, std::function<double(double, double, double)> const& func2, unsigned iterations, double x0, double y0, double t0 = 0.0, double stepSize = 0.5)
{
	std::list<std::tuple<double, double, double>> result;

	double x = x0, y = y0, t = t0;

	result.emplace_back(std::make_tuple(t, x, y));

	for (unsigned n = 1; n <= iterations; ++n)
	{
		double K1x = func1(t, x, y);
		double K1y = func2(t, x, y);
		double K2x = func1(t + stepSize, x + stepSize * K1x, y + stepSize * K1y);
		double K2y = func2(t + stepSize, x + stepSize * K1x, y + stepSize * K1y);
		x += stepSize * 0.5 * (K1x + K2x);
		y += stepSize * 0.5 * (K1y + K2y);
		t += stepSize;

		result.emplace_back(std::make_tuple(t, x, y));
	}

	return result;
}

std::list<std::pair<double, double>> rungekutta(std::function<double(double, double)> const& func, unsigned iterations, double y0, double t0 = 0.0, double stepSize = 0.5)
{
	std::list<std::pair<double, double>> result;

	double y = y0, t = t0;

	result.emplace_back(std::make_pair(t, y));

	for (unsigned n = 1; n <= iterations; ++n)
	{
		double K1 = func(t, y);
		double K2 = func(t + stepSize * 0.5, y + stepSize * 0.5 * K1);
		double K3 = func(t + stepSize * 0.5, y + stepSize * 0.5 * K2);
		double K4 = func(t + stepSize, y + stepSize * K3);
		y += stepSize / 6.0 * (K1 + 2.0 * K2 + 2.0 * K3 + K4);
		t += stepSize;

		result.emplace_back(std::make_pair(t, y));
	}

	return result;
}

std::list<std::tuple<double, double, double>> rungekutta(std::function<double(double, double, double)> const& func1, std::function<double(double, double, double)> const& func2, unsigned iterations, double x0, double y0, double t0 = 0.0, double stepSize = 0.5)
{
	std::list<std::tuple<double, double, double>> result;

	double x = x0, y = y0, t = t0;

	result.emplace_back(std::make_tuple(t, x, y));

	for (unsigned n = 1; n <= iterations; ++n)
	{
		double K1x = func1(t, x, y);
		double K1y = func2(t, x, y);
		double K2x = func1(t + stepSize * 0.5, x + stepSize * 0.5 * K1x, y + stepSize * 0.5 * K1y);
		double K2y = func2(t + stepSize * 0.5, x + stepSize * 0.5 * K1x, y + stepSize * 0.5 * K1y);
		double K3x = func1(t + stepSize * 0.5, x + stepSize * 0.5 * K2x, y + stepSize * 0.5 * K2y);
		double K3y = func2(t + stepSize * 0.5, x + stepSize * 0.5 * K2x, y + stepSize * 0.5 * K2y);
		double K4x = func1(t + stepSize, x + stepSize * K3x, y + stepSize * K3y);
		double K4y = func2(t + stepSize, x + stepSize * K3x, y + stepSize * K3y);
		x += stepSize / 6.0 * (K1x + 2.0 * K2x + 2.0 * K3x + K4x);
		y += stepSize / 6.0 * (K1y + 2.0 * K2y + 2.0 * K3y + K4y);
		t += stepSize;

		result.emplace_back(std::make_tuple(t, x, y));
	}

	return result;
}

static void glfwErrorCallback(int error, const char* description)
{
	fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

#pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"")

int main()
{
	auto func = [](double t, double y)
	{
		return y - t + pow(2, -y);
	};

	auto func1 = [](double t, double x, double y)
	{
		return x - 0.1 * x * y;
	};

	auto func2 = [](double t, double x, double y)
	{
		return -0.5 * y + 0.01 * x * y;
	};

	auto func3 = [](double t, double x, double v)
	{
		return v;
	};

	auto func4 = [](double t, double y, double v)
	{
		return t - y - v;
	};

	std::ofstream file;

	std::function<void(unsigned)> problems[3] = {
		// First Order ODE
		[&func, &file](unsigned method)
		{
			static std::function<std::list<std::pair<double, double>>(std::function<double(double, double)> const&, unsigned, double, double, double)> methods[4] =
			{
				[](std::function<double(double, double)> const& func, unsigned iterations, double y0, double t0 = 0.0, double stepSize = 0.5)
				{
					return euler(func, iterations, y0, t0, stepSize);
				},
				[](std::function<double(double, double)> const& func, unsigned iterations, double y0, double t0 = 0.0, double stepSize = 0.5)
				{
					return midpoint(func, iterations, y0, t0, stepSize);
				},
				[](std::function<double(double, double)> const& func, unsigned iterations, double y0, double t0 = 0.0, double stepSize = 0.5)
				{
					return modifiedEuler(func, iterations, y0, t0, stepSize);
				},
				[](std::function<double(double, double)> const& func, unsigned iterations, double y0, double t0 = 0.0, double stepSize = 0.5)
				{
					return rungekutta(func, iterations, y0, t0, stepSize);
				}
			};

			file.open("data.txt", std::ofstream::out | std::ofstream::trunc);

			if (!file.is_open())
				return;

			auto data = methods[method](func, 100, 1, 0, 0.05);

			bool first = true;
			std::string x("["), y("[");

			for (auto& pair : data)
			{
				if (!first)
				{
					x += ",";
					y += ",";
				}

				x += std::to_string(pair.first);
				y += std::to_string(pair.second);
				first = false;
			}

			x += "]";
			y += "]";

			file << x << std::endl << y;

			file.flush();
			file.close();
		},

		// System of ODEs
		[&func1, &func2, &file](unsigned method)
		{
			static std::function<std::list<std::tuple<double, double, double>>(std::function<double(double, double, double)> const&, std::function<double(double, double, double)> const&, unsigned, double, double, double, double)> methods[4] =
			{
				[](std::function<double(double, double, double)> const& func1, std::function<double(double, double, double)> const& func2, unsigned iterations, double x0, double y0, double t0 = 0.0, double stepSize = 0.5)
				{
					return euler(func1, func2, iterations, x0, y0, t0, stepSize);
				},
				[](std::function<double(double, double, double)> const& func1, std::function<double(double, double, double)> const& func2, unsigned iterations, double x0, double y0, double t0 = 0.0, double stepSize = 0.5)
				{
					return midpoint(func1, func2, iterations, x0, y0, t0, stepSize);
				},
				[](std::function<double(double, double, double)> const& func1, std::function<double(double, double, double)> const& func2, unsigned iterations, double x0, double y0, double t0 = 0.0, double stepSize = 0.5)
				{
					return modifiedEuler(func1, func2, iterations, x0, y0, t0, stepSize);
				},
				[](std::function<double(double, double, double)> const& func1, std::function<double(double, double, double)> const& func2, unsigned iterations, double x0, double y0, double t0 = 0.0, double stepSize = 0.5)
				{
					return rungekutta(func1, func2, iterations, x0, y0, t0, stepSize);
				}
			};

			file.open("data.txt", std::ofstream::out | std::ofstream::trunc);

			if (!file.is_open())
				return;

			auto data = methods[method](func1, func2, 100, 100, 8, 0, 0.05);

			bool first = true;
			std::string x("["), y1("["), y2("[");

			for (auto& t : data)
			{
				if (!first)
				{
					x += ",";
					y1 += ",";
					y2 += ",";
				}

				x += std::to_string(std::get<0>(t));
				y1 += std::to_string(std::get<1>(t));
				y2 += std::to_string(std::get<2>(t));
				first = false;
			}

			x += "]";
			y1 += "]";
			y2 += "]";

			file << x << std::endl << y1 << std::endl << x << std::endl << y2;

			file.flush();
			file.close();
		},

		// Second Order ODE
		[&func3, &func4, &file](unsigned method)
		{
			static std::function<std::list<std::tuple<double, double, double>>(std::function<double(double, double, double)> const&, std::function<double(double, double, double)> const&, unsigned, double, double, double, double)> methods[4] =
			{
				[](std::function<double(double, double, double)> const& func1, std::function<double(double, double, double)> const& func2, unsigned iterations, double x0, double y0, double t0 = 0.0, double stepSize = 0.5)
				{
					return euler(func1, func2, iterations, x0, y0, t0, stepSize);
				},
				[](std::function<double(double, double, double)> const& func1, std::function<double(double, double, double)> const& func2, unsigned iterations, double x0, double y0, double t0 = 0.0, double stepSize = 0.5)
				{
					return midpoint(func1, func2, iterations, x0, y0, t0, stepSize);
				},
				[](std::function<double(double, double, double)> const& func1, std::function<double(double, double, double)> const& func2, unsigned iterations, double x0, double y0, double t0 = 0.0, double stepSize = 0.5)
				{
					return modifiedEuler(func1, func2, iterations, x0, y0, t0, stepSize);
				},
				[](std::function<double(double, double, double)> const& func1, std::function<double(double, double, double)> const& func2, unsigned iterations, double x0, double y0, double t0 = 0.0, double stepSize = 0.5)
				{
					return rungekutta(func1, func2, iterations, x0, y0, t0, stepSize);
				}
			};

			file.open("data.txt", std::ofstream::out | std::ofstream::trunc);

			if (!file.is_open())
				return;

			auto data = methods[method](func3, func4, 100, 1, 0, 0, 0.05);

			bool first = true;
			std::string x("["), y("[");

			for (auto& t : data)
			{
				if (!first)
				{
					x += ",";
					y += ",";
				}

				x += std::to_string(std::get<1>(t));
				y += std::to_string(std::get<2>(t));
				first = false;
			}

			x += "]";
			y += "]";

			file << x << std::endl << y;

			file.flush();
			file.close();
		}
	};

	// Intitialize GLFW and GLEW
	glfwSetErrorCallback(glfwErrorCallback);

	if (!glfwInit())
		return 1;

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

	GLFWwindow* window = glfwCreateWindow(230, 150, "Differential Equation Solver - MAT357 Prg2", nullptr, nullptr);
	if (!window)
		return 1;

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	if (glewInit() != GLEW_OK)
	{
		std::cerr << "Failed to initialize OpenGL." << std::endl;
		return 1;
	}

	int width, height;
	glfwGetFramebufferSize(window, &width, &height);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::GetIO().IniFilename = nullptr;
	ImGui::StyleColorsDark();

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 130");

	glClearColor(0.95f, 0.95f, 0.95f, 1.0f);

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		glfwGetFramebufferSize(window, &width, &height);
		glViewport(0, 0, width, height);

		glClear(GL_COLOR_BUFFER_BIT);

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGui::SetNextWindowPos({ 10, 10 }, ImGuiCond_Once);
		ImGui::SetNextWindowSize({ 210, 100 }, ImGuiCond_Once);
		ImGui::Begin("", 0, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);
		static int problem = 0, method = 0;
		ImGui::Combo("Problem", &problem, "First Order ODE\0System of ODEs\0Second Order ODE\0");
		ImGui::Combo("Method", &method, "Euler\0Midpoint\0Modified Euler\0Runge-Kutta\0");
		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::SameLine(ImGui::GetWindowWidth() - (50.0f + ImGui::GetStyle().ItemSpacing.x));
		if (ImGui::Button("Plot"))
			problems[problem](method);
		ImGui::End();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		
		glfwSwapBuffers(window);
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}
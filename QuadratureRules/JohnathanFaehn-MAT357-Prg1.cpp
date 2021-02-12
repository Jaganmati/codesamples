#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <random>
#include <utility>

class Solver
{
    Solver() = delete;
    
    static double midpoint(std::function<double(double)> const& func, double a, double b, unsigned n, double h)
    {
        double result = 0.0;
        for (unsigned k = 1; k <= n; ++k)
            result += func((2.0 * a + (2.0 * k - 1.0) * h) * 0.5);
        
        return h * result;
    }
    
    static double trapezoid(std::function<double(double)> const& func, double a, double b, unsigned n, double h)
    {
        double result = 0.0;
        for (unsigned k = 1; k < n; ++k)
            result += func(a + k * h);
        
        return (h / 2.0) * (func(a) + func(b) + 2.0 * result);
    }
    
    static double simpson(std::function<double(double)> const& func, double a, double b, unsigned n, double h)
    {
        double result = 0.0;
        for (unsigned k = 1; k <= n; ++k)
            result += (k != n ? 2.0 * func(a + k * h) : 0.0) + 4.0 * func((2.0 * a + (2.0 * k - 1.0) * h) * 0.5);
        
        return (h / 6.0) * (func(a) + func(b) + result);
    }
    
    public:
        enum NumericalMethod
        {
            Midpoint,
            Trapezoid,
            Simpsons
        };
        
        static std::pair<double, unsigned> solve(NumericalMethod method, std::function<double(double)> const& func, double a, double b, double eps, unsigned k = 1)
        {
            if (b - a && eps)
            {
                static const std::function<double(std::function<double(double)> const&, double, double, unsigned, double)> s[] = { midpoint, trapezoid, simpson };
                const unsigned index = static_cast<unsigned>(method);
                
                unsigned n = 1, iterations = 1;
                double diff = b - a, h = diff / n, result = s[index](func, a, b, n, h), prev = 0.0;
                
                do
                {
                    h = diff / (n += k);
                    prev = result;
                    result = s[index](func, a, b, n, h);
                    ++iterations;
                }
                while (std::abs(result - prev) > eps);
                
                return std::make_pair(result, iterations);
            }
            
            return std::make_pair(0.0, 0U);
        }
        
        static std::pair<double, unsigned> montecarlo(std::function<double(double)> const& func, double a, double b, double c, double d, unsigned initialPoints, double eps, unsigned k = 1)
        {
            double width = b - a, height = d - c;
            
            if (initialPoints && width && height)
            {
                static std::mt19937 random(std::chrono::system_clock::now().time_since_epoch().count());
                
                double area = width * height;
                unsigned K = 0, N = initialPoints, iterations = 1;
                
                for (unsigned i = 0; i < initialPoints; ++i)
                {
                    double x = a + (static_cast<double>(random()) / random.max()) * width;
                    double y = c + (static_cast<double>(random()) / random.max()) * height;
                    
                    K += func(x) >= y;
                }
                
                double result = ((area * K) / N), prev = 0.0;
                
                do
                {
                    prev = result;
                    
                    for (unsigned i = 0; i < k; ++i)
                    {
                        double x = a + (static_cast<double>(random()) / random.max()) * width;
                        double y = c + (static_cast<double>(random()) / random.max()) * height;
                        
                        K += func(x) >= y;
                    }
                    
                    ++iterations;
                    result = (area * K) / (N += k);
                }
                while (std::abs(result - prev) > eps);
                
                return std::make_pair(result, iterations);
            }
            
            return std::make_pair(0.0, 0U);
        }
};

int main()
{
    static const std::function<double(double)> funcs[] =
    {
        [](double x) -> double
        {
            return exp(-2.0 * pow(x, 8.0)) + pow(x, M_E) - pow(x, x);
        },
        [](double x) -> double
        {
            return pow(x, 1.0 / 3.0 * pow(x, 2.0)) * 0.25 - 2.0 * pow(x, exp(2.0)) + pow(x, pow(x, -1.0));
        },
        [](double x) -> double
        {
            return pow(x, 3.0 * x) + exp(pow(x, 16.0)) - 1.0;
        }
    };
    
    static double topEdges[] = { 0.5, 0.7, 1.0 };
    
    for (unsigned i = 0, last = sizeof(funcs) / sizeof(funcs[0]); i < last; ++i)
    {
        if (i) std::cout << std::endl;
        std::cout << "Function " << (i + 1) << std::endl;
        auto midpoint = Solver::solve(Solver::Midpoint, funcs[i], 0, 1, pow(10, -7), 100);
        std::cout << "Midpoint: " << std::to_string(midpoint.first) << " (" << midpoint.second << " iterations)" << std::endl;
        auto trapezoid = Solver::solve(Solver::Trapezoid, funcs[i], 0, 1, pow(10, -7), 100);
        std::cout << "Trapezoid: " << std::to_string(trapezoid.first) << " (" << trapezoid.second << " iterations)" << std::endl;
        auto simpsons = Solver::solve(Solver::Simpsons, funcs[i], 0, 1, pow(10, -7), 100);
        std::cout << "Simpsons: " << std::to_string(simpsons.first) << " (" << simpsons.second << " iterations)" << std::endl;
        auto montecarlo = Solver::montecarlo(funcs[i], 0, 1, 0, topEdges[i], 1000, pow(10, -7), 100);
        std::cout << "Monte Carlo: " << std::to_string(montecarlo.first) << " (" << montecarlo.second << " iterations)" << std::endl;
    }
    
    return 0;
}

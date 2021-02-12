#define NOMINMAX

#include <array>
#include <atomic>
#include <cstdio>
#include <functional>
#include <iostream>
#include <iterator>
#include <list>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#include <windows.h>

#include "BigInt.h"

std::thread funcThread;
std::atomic_bool calculating = false, running = true;

__declspec(noinline) BOOL WINAPI signalHandler(DWORD ctrlType)
{
    if (ctrlType == CTRL_C_EVENT)
    {
        // This can cause problems if it terminates at the wrong time, like when it's allocating memory from the heap.
        // To reduce the chance of mismanagement, a state should be periodically checked and gracefully close a thread
        // when that state is set to tell the thread to close. This check would take extra cycles, and would clutter
        // the code. Any mismanagement will be cleaned up after closing the process, so this is a minimal concern for
        // this application's use cases.
        if (TerminateThread(funcThread.native_handle(), 0) && calculating)
            std::cerr << "The operation was cancelled by the user." << std::endl << std::endl;

        return TRUE;
    }

    return FALSE;
}

// Forward declaration
std::string evaluate(const std::string& str);

// Integral modulo
template <typename T>
T mod(T a, T b)
{
    if (a < 0)
    {
        a += b * (-a / b);

        if (a < 0)
            a += b;

        return a;
    }

    return a % b;
}

// ax + by = gcd(a, b)
template <typename T>
std::list<std::array<T, 5>> euler(T a, T b)
{
    std::list<std::array<T, 5>> result;
    std::array<T, 4> matrix = { 1, 0, 0, 1 };
    
    T q = 0, r = 0;
    
    do
    {
        q = a / b;
        r = a - q * b;
        
        result.emplace_back(std::array<T, 5>({ a, q, b, r, 0 }));
        
        a = b;
        b = r;
        
        if (r != 0)
        {
            T m0 = matrix[0], m1 = matrix[1];
            matrix[0] = matrix[2];
            matrix[1] = matrix[3];
            matrix[2] = m0 - q * matrix[2];
            matrix[3] = m1 - q * matrix[3];
        }
    }
    while (r != 0);
    
    auto& front = result.front();
    result.emplace_back(std::array<T, 5>({ front[0] != 0 && front[2] % front[0] != 0 ? matrix[2] : 1, front[0], front[2] != 0 && front[0] % front[2] != 0 ? matrix[3] : 1, front[2], result.size() > 1 ? (*++result.rbegin())[3] : front[2] != 0 && front[0] % front[2] != 0 ? front[0] : front[2] }));
    
    return result;
}

template <typename T>
T euler_lite(T a, T b)
{
    T q = 0, r = 1, pr = 0;

    do
    {
        pr = r;
        q = a / b;
        r = a - q * b;

        a = b;
        b = r;
    } while (r != 0);

    return pr;
}

// Primes
template <typename T>
std::list<T> sieve(T a, T b)
{
    bool altb = a < b;
    T max = altb ? b : a;
    T min = altb ? a : b;

    static std::list<T> primes;

    if (primes.empty() || ((primes.back() + 1) * (primes.back() + 1)) <= max)
    {
        for (T prime = primes.empty() ? 2 : primes.back() + 1; prime * prime <= max; ++prime)
        {
            for (T const& p : primes)
                if (prime % p == 0)
                    goto nextPrime;

            primes.emplace_back(prime);

            nextPrime:
            continue;
        }
    }

    std::list<T> result;
    for (T num = min; num <= max; ++num)
    {
        for (T const& prime : primes)
        {
            T abs = num < 0 ? -num : num;
            if (abs == prime)
                break;
            else if (abs == 1 || num % prime == 0)
                goto nextResult;
        }

        result.emplace_back(num);

        nextResult:
        continue;
    }

    return result;
}

// Prime factors
template <typename T>
std::list<std::pair<T, T>> trialdivision(T num)
{
    std::list<std::pair<T, T>> result;

    T stepSize = 10000;
    T r1 = 1, r2 = stepSize;
    T max = T(10) * T(10) * T(10) * T(10) * T(10);// *T(10);// * T(10);

    while ((num < 0 ? -num : num) > 1)
    {
        if (r1 >= max)
        {
            result.emplace_back(std::pair<T, T>(num, 1));
            break;
        }

        auto primes = sieve(r1, r2);

        for (T& prime : primes)
        {
            if (num % prime == 0)
            {
                T exp = 0;

                do
                {
                    ++exp;
                    num /= prime;
                } while (num % prime == 0);

                result.emplace_back(std::pair<T, T>(prime, exp));

                if ((num < 0 ? -num : num) == 1)
                    break;
            }
        }

        r1 += stepSize;
        r2 += stepSize;
    }

    return result;
}

// ax + by = c
template <typename T>
std::pair<std::pair<T, T>, std::pair<T, T>> lineareqsolver(T a, T b, T c)
{
    auto e = euler(a, b);
    auto gcd = e.back()[4];

    if (c % gcd != 0)
    {
        std::cerr << "No solutions: gcd(" << a << ", " << b << ") = " << gcd << ", which does not divide " << c << std::endl;
        throw std::runtime_error("");
    }

    if (gcd != 1)
    {
        a /= gcd;
        b /= gcd;
        c /= gcd;

        //e = euler(a, b);
        //gcd = e.back()[4];
    }

    T x0 = e.back()[0] * c;
    T y0 = e.back()[2] * c;

    T& x = x0;
    T& xt = b;
    T& y = y0;
    T yt = -a;

    return std::pair<std::pair<T, T>, std::pair<T, T>>(std::pair<T, T>(x, xt), std::pair<T, T>(y, yt));
}

// ax == 1 (mod m)
template <typename T>
T multinv(T a, T m)
{
    auto e = euler(a, m);
    auto gcd = e.back()[4];

    if (gcd != 1)
    {
        std::cerr << "No solution: gcd(" << a << ", " << m << ") = " << gcd << ", when a result of 1 is required." << std::endl;
        throw std::runtime_error("");
    }

    T inv = e.back()[0];

    while (inv < 0)
        inv += m;

    return inv;
}

// TODO: Congruence solver

// a^x == M (mod m)
template <typename T>
T modexp(T a, T x, T m)
{
    T M = 1;

    for (T exp = 1, e = x, f = e / 2, r = e - f * 2, A = a % m; exp <= x; exp *= 2, e = f, f = e / 2, r = e - f * 2, A = (A * A) % m)
    {
        //std::cout << e << " / 2 = " << f << ", r = " << r << std::endl;
        //std::cout << a << "^" << exp << " == " << A << " (mod " << m << ")" << std::endl;

        if (r == 1)
        {
            //std::cout << A << " * M -> M == ";
            M = mod((M * A), m);
            //std::cout << M << " (mod " << m << ")" << std::endl;
        }

        //std::cout << std::endl;
    }

    return M;
}

// System of x == a (mod m) solver
template <typename T>
std::pair<T, T> chinrem(std::list<std::pair<T, T>> m)
{
    T M = 1;

    std::list<T> Mn(m.size(), 1);
    std::list<T> b(m.size());

    auto iter = m.begin();
    for (unsigned ind = 0; ind < m.size(); ++ind, iter = std::next(iter))
    {
        M *= iter->second;

        auto MnIter = Mn.begin();
        for (unsigned i = 0; i < m.size(); ++i, MnIter = std::next(MnIter))
        {
            if (i != ind)
                *MnIter *= iter->second;
        }
    }

    T x0 = 0;
    auto MnIter = Mn.begin();
    for (iter = m.begin(); iter != m.end(); iter = std::next(iter), MnIter = std::next(MnIter))
    {
        x0 += iter->first * multinv(*MnIter, iter->second) * *MnIter;
    }

    return std::pair<T, T>(x0 % M, M);
}

// A test for whether or not a number is prime
template <typename T>
bool wilsonsprimetest(T n)
{
    T m = 1;

    for (T i = 2; i < n; ++i)
        m = (i * m) % n;

    return m == n - 1;
}

// Finds x in a^x || b!
template <typename T>
T factors(T a, T b)
{
    T c = 0;

    while (a <= b)
    {
        //std::cout << "C: " << c << " => ";
        c += b / a;
        //std::cout << c << std::endl;
        //std::cout << "A: " << a << " => ";
        a *= a;
        //std::cout << a << std::endl;
    }

    return c;
}

template <typename T>
T phi(T n)
{
    T result = n;

    auto primes = trialdivision(n);

    for (auto& prime : primes)
    {
        auto& p = prime.first;
        result *= p - 1;
        result /= p;
    }

    return result;
}

// Finds x in a^x == 1 (mod m)
template <typename T>
T order(T a, T m)
{
    if (a == 0 && euler_lite(a, m) == 1)
        return T(1);

    T p = phi(m);
    T q = p;
    T d = 1;

    std::list<T> divisors;

    while (q > 1)
    {
        while ((p % ++d) > 0);

        divisors.emplace_back(q = p / d);
    }

    for (auto it = divisors.begin(); it != divisors.end(); ++it)
    {
        if (modexp(a, *it, m) == 1)
        {
            p = *it;
        }
        else
        {
            for (auto next = std::next(it); next != divisors.end();)
            {
                if ((*it % *next) == 0)
                    next = divisors.erase(next);
                else ++next;
            }
        }
    }

    return p;
}

template <typename T>
std::list<T> primroots(T const& m, T const& c)
{
    std::list<T> roots;

    auto ord = [](T const& a, T const& m, T p, std::list<T> divisors)
    {
        if (a == 0 && euler_lite(a, m) == 1)
            return T(1);

        for (auto it = divisors.begin(); it != divisors.end(); ++it)
        {
            if (modexp(a, *it, m) == 1)
            {
                p = *it;
            }
            else
            {
                for (auto next = std::next(it); next != divisors.end();)
                {
                    if ((*it % *next) == 0)
                        next = divisors.erase(next);
                    else ++next;
                }
            }
        }

        return p;
    };

    T p = phi(m);
    T q = p;
    T d = 1;

    std::list<T> divisors;

    while (q > 1)
    {
        while (p % ++d > 0);

        divisors.emplace_back(q = p / d);
    }

    for (T a = 1, count = c < 0 ? phi(p) : std::min(phi(p), c); a < m && count > roots.size(); ++a)
    {
        if (euler_lite(a, m) == 1 && ord(a, m, p, divisors) == p)
            roots.emplace_back(a);
    }

    return roots;
}

template <typename T>
__declspec(noinline) std::list<T> getDivisors(T p)
{
    T q = p, d = 1;

    std::list<T> divisors;
    divisors.emplace_front(p);

    while (q > 1)
    {
        while ((p % ++d) > 0);

        divisors.emplace_front(q = p / d);
    }

    return divisors;
}

// Brute force solves for x in p^x == a (mod m)
template <typename T>
T index(T a, T p, T m)
{
    for (T x = 1, rr = 1; x < m; ++x)
    {
        rr = mod(p * rr, m);
        if (rr == a)
            return x;
    }

    std::cerr << "No x exists such that " << p << "^x == " << a << " (mod " << m << ")." << std::endl;
    throw std::runtime_error("");
}

// Solves for x in p^x == a (mod m) using the Baby step-Giant step algorithm
template <typename T>
T babygiant(T a, T p, T m)
{
    T N = std::nthrooti(m - 1, T(2));

    if (N * N < m - 1)
        ++N;

    //std::cout << "N = " << N << std::endl;

    T R = multinv(modexp(p, N, m), m);

    std::map<T, T> list1, list2;

    for (T i = 0, pp = 1, rr = 1; i < N; ++i, pp = modexp(p, i, m), rr = modexp(R, i, m))
    {
        list1.emplace(pp, i);
        list2.emplace(mod(a * rr, m), i);
    }

    for (auto it = list1.begin(); it != list1.end(); ++it)
    {
        auto iter = list2.find(it->first);
        if (iter != list2.end())
        {
            std::cout << "i of list 1 = " << it->second << std::endl;
            std::cout << "j of list 2 = " << iter->second << std::endl;
            std::cout << "matching entry = " << it->first << std::endl;
            return it->second + iter->second * N;
        }
    }

    std::cerr << "No x exists such that " << p << "^x == " << a << " (mod " << m << ")." << std::endl;
    throw std::runtime_error("");
}

// Determines whether a number is prime relative to a base
template <typename T>
std::tuple<bool, T, T> strongfermattest(T const& N, T const& b)
{
    T k = 0, s = N - 1;

    while (s > 0 && (s & 1) == 0)
    {
        ++k;
        s /= 2;
    }

    T Nmin1;
    T b0 = modexp(b, s, N);
    if (b0 == 1 || b0 == (Nmin1 = N - 1))
        return std::make_tuple(true, k, s);

    for (T i = 1, bi = modexp(b0, T(2), N); i < k; ++i, bi = modexp(bi, T(2), N))
    {
        if (bi == Nmin1)
            return std::make_tuple(true, k, s);
    }

    return std::make_tuple(false, k, s);
}

// Determines whether a number is a probable prime
template <typename T>
bool rabinmiller(T const& N)
{
    std::mt19937 random;
    std::set<T> checked;

    for (T i = 0, Nmin1 = N - 1; i < 200 && Nmin1 > checked.size(); ++i)
    {
        T b;

        do
        {
            b = mod(T(random()), N - 2) + 2;
        } while (checked.find(b) != checked.end());

        if (std::get<0>(strongfermattest(N, b)))
            return true;

        checked.emplace(b);
    }

    return false;
}

template <typename T>
std::list<std::pair<T, T>> fermatfactor(T N)
{
    std::list<std::pair<T, T>> result;
    std::map<T, T> factors;

    T steps = 0;

    while (N != 0 && (N & 1) == 0)
    {
        ++factors[T(2)];
        N /= 2;
        ++steps;
    }

    if (rabinmiller(N))
    {
        ++factors[N];

        for (auto const& f : factors)
            result.emplace_back(f.first, f.second);

        return result;
    }

    T sq = std::nthrooti(N, T(2));
    T b2 = sq * sq - N;

    if (b2 < 0)
    {
        b2 += sq * 2 + 1;
        ++sq;
    }

    ///T b = std::nthrooti(b2, T(2));

    for (; !std::issquare(b2)/*b * b < b2*/; b2 += sq * 2 + 1, ++sq/*, b = std::nthrooti(b2, T(2))*/, ++steps);
    ++steps;

    T b = std::nthrooti(b2, T(2));

    std::cout << "a = " << sq << ", b = " << b << ", found in " << steps << " steps" << std::endl;

    T l = sq - b;
    if (l != 1)
    {
        /*if (l != N)
        {
            for (auto const& f : fermatfactor(l))
                factors[f.first] += f.second;
        }
        else */++factors[l];
    }

    T r = sq + b;
    if (r != 1)
    {
        /*if (r != N)
        {
            for (auto const& f : fermatfactor(r))
                factors[f.first] += f.second;
        }
        else */++factors[r];
    }

    for (auto const& f : factors)
        result.emplace_back(f.first, f.second);

    return result;
}

template <typename T>
std::array<T, 5> pollardrho(T const& N, std::string const& func, T const& x0)
{
    static auto fn = [](std::string func, T const& x) -> T {
        
        // TODO: Replace with variable support in evaluate to avoid replacing "x" in function names and handle implicity multiplication.
        for (size_t i = func.find('x'); i != func.npos; i = func.find('x'))
            func = func.substr(0, i) + std::to_string(x) + func.substr(i + 1);

        std::stringstream ss(evaluate(func));

        T result;
        ss >> result;
        return result;
    };

    T x = x0, y = fn(func, fn(func, x0)), steps = 1, gcd;

    while ((gcd = euler_lite(x < y ? y - x : x - y, N)) == 1)
    {
        x = mod(fn(func, x), N);
        y = mod(fn(func, fn(func, y)), N);
        ++steps;
    }

    if (gcd == N)
    {
        std::cerr << "No result found. Either the function or starting x should be adjusted." << std::endl;
        throw std::runtime_error("");
    }

    return { steps, x, y, gcd, N / gcd };
}

template <typename T>
std::array<T, 5> pollardp_1(T const& N, T a, T const& M)
{
    for (T i = 2, exp = 2, b = a; i <= M; exp = exp * ++i)
    {
        b = mod(modexp(b, exp, N) - 1, N);
        T gcd = euler_lite(b, N);

        if (gcd != 1 && gcd != N)
            return { a, T(i), N, gcd, N / gcd };
    }

    std::cerr << "No result found. Either the base \"a\" or upperbound \"M\" should be updated." << std::endl;
    throw std::runtime_error("");
}

template <typename T>
bool lehmertest(T const& N)
{
    if (rabinmiller(N))
    {
        T Nmin1 = N - 1;
        auto primedivisors = trialdivision(Nmin1);
        T lastdivisor = primedivisors.back().first;

        if (lastdivisor < 100'000 || lehmertest(lastdivisor))
        {
            auto roots = primroots(std::stoll(std::to_string(N)), 1LL);

            T b = std::to_string(roots.front());

            if (modexp(b, Nmin1, N) != 1)
                return false;

            for (auto divisor : primedivisors)
            {
                T& q = divisor.first;

                if (modexp(b, Nmin1 / q, N) == 1)
                    return false;
            }

            return true;
        }
        else
        {
            // TODO: Attempt this again using another factoring method, such as pollardrho.
            std::cerr << "No result found. TrialDivision is not able to fully factor " << N << " - 1." << std::endl;
            throw std::runtime_error("");
        }
    }

    return false;
}

template <typename T>
std::list<std::tuple<T, T, T>> continuedfractionsqrt(T const& N, T const& depth)
{
    std::list<std::tuple<T, T, T>> result;

    T flsqrtN = std::nthrooti(N, T(2));

    T An = 0;
    T Bn = 1;
    T an = flsqrtN;

    result.emplace_back(An, Bn, an);

    for (T n = 0; n < depth; ++n)
    {
        An = an * Bn - An;
        Bn = (N - An * An) / Bn;
        an = (An + flsqrtN) / Bn;

        result.emplace_back(An, Bn, an);
    }

    return result;
}

template <typename T>
T quadres(T a, T const& b)
{
    if ((b & T(1)) == 1)
    {
        if (lehmertest(b))
        {
            T c = modexp(a, (b - 1) / 2, b);
            return c == 1 ? 1 : -1;
        }

        if (b > 0)
        {
            if (a == -1 || a == b - 1)
            {
                T c = mod(b, T(4));
                if (c == 1)
                    return 1;
                if (c == 3)
                    return -1;
            }

            if (a == 2)
            {
                T c = mod(b, T(8));
                if (c == 1 || c == 7)
                    return 1;
                if (c == 3 || c == 5)
                    return -1;
            }

            if (std::issquare(a))
                return 1;
        }
        else
        {
            T l, r;
            if ((l = mod(a, T(4))) == 1 || (r = mod(b, T(4))) == 1)
                return quadres(mod(b, a), a);
            else if (l == 3 && r == 3)
                return -quadres(mod(b, a), a);
        }
    }
    else if (b == 2)
    {
        return mod(a, b) == 1 ? 1 : -1;
    }
    else if ((a & T(1)) == 1)
    {
        T l, r;
        if ((l = mod(a, T(4))) == 1 || (r = mod(b, T(4))) == 1)
            return quadres(b, a);
        else if (l == 3 && r == 3)
            return -quadres(b, a);
    }

    T result = 1;
    auto factors = trialdivision(b);
    for (auto& entry : factors)
    {
        auto& factor = entry.first;
        auto& exp = entry.second;

        result *= std::powi(quadres(a, factor), exp);
    }

    return result;
}

// solves for x in x^2 == a (mod p) when p is an odd prime
template <typename T>
T tonelli_shanks(T const& a, T const& p)
{
    // Verify p is an odd prime.
    if ((p & T(1)) == 0 || !rabinmiller(p)/* || !wilsonsprimetest(p)*/) // TODO: Use Lehmer's test for the prime check.
    {
        std::cerr << "No solution: p must be an odd prime." << std::endl;
        throw std::runtime_error("");
    }

    // Verify (a/p) = 1 using Euler's Criterion because p is an odd prime.
    if (modexp(a, (p - 1) / 2, p) != 1)
    {
        std::cerr << "No solution: (a/p) must be 1, which is not the case." << std::endl;
        throw std::runtime_error("");
    }

    T k = 0, m = p - 1;
    for (; m % 2 == 0; ++k, m /= 2);

    T n = 2, o = mod(p, T(8));

    if (o != 3 && o != 5)
    {
        ++n;
        for (T pmin1 = p - 1; std::issquare(n) || modexp(n, pmin1 / 2, p) != pmin1; ++n);
    }

    T c = modexp(n, m, p);
    T r = modexp(a, (m + 1) / 2, p);
    T t = modexp(a, m, p);

    T i = 0;

    while (t != 1 && k != 1)
    {
        T ot = order(t, p);
        T ki_1 = 0;
        for (; ot % 2 == 0; ++ki_1, ot /= 2);

        T b = modexp(c, std::powi(T(2), (k - ki_1) - 1), p);

        std::cout << "i = " << i++ << ", c = " << c << ", k = " << k << ", r = " << r << ", t = " << t << std::endl;

        c = mod(b * b, p);
        t = mod(b * b * t, p);
        r = mod(b * r, p);
        k = ki_1;
    }

    std::cout << "i = " << i << ", c = " << c << ", k = " << k << ", r = " << r << ", t = " << t << std::endl;

    return r;
}

template <typename T>
T quadeqsolver(T const& a, T const& p)
{
    if (quadres(a, p) == 1)
    {
        if (mod(p, T(4)) == 3)
            return modexp(a, (p + 1) / 4, p);

        if (mod(p, T(8)) == 5)
        {
            T x = modexp(a, (p + 3) / 8, p);

            if (modexp(x, T(2), p) == mod(a, p))
            {
                return x;
            }

            return mod(modexp(T(2), (p - 1) / 4, p) * x, p);
        }
    }

    return tonelli_shanks(a, p);
}

template <typename T>
std::list<std::tuple<T, T, T, T, std::list<std::pair<T, T>>>> continuedfractionfactor(T const& N, std::list<T> const& base, T const& depth)
{
    // TODO: Sort the bases, and verify all are prime.

    T flsqrtN = std::nthrooti(N, T(2));

    T An = 0;
    T Bn = 1;
    T an = flsqrtN;

    std::list<std::tuple<T, T, T, T, std::list<std::pair<T, T>>>> fullyFactored;

    for (T n = 0; depth > fullyFactored.size(); ++n)
    {
        An = an * Bn - An;
        Bn = (N - An * An) / Bn;
        an = (An + flsqrtN) / Bn;

        std::list<std::pair<T, T>> factors;

        T M = Bn;
        for (T const& b : base)
        {
            T e = 0;
            while (M % b == 0)
            {
                ++e;
                M /= b;
            }

            if (e > 0)
                factors.emplace_back(b, e);

            if (M == 1)
                break;
        }

        if (M == 1)
        {
            // p_n^2 == (-1)^(n+1)*B_(n+1) (mod N)
            T mul = std::powi(T(-1), n + 1);
            T pn = quadeqsolver(mod(mul * Bn, N), N);

            fullyFactored.emplace_back(n, pn, Bn, (mul + 1) / 2, factors);
        }
    }

    return fullyFactored;
}

// Ciphers

// Ceasar (shift) cipher/decipher
std::list<std::tuple<char, unsigned, unsigned, char>> ceasar(int b, std::string const& str)
{
    std::list<std::tuple<char, unsigned, unsigned, char>> result;

    for (auto iter = str.begin(); iter != str.end(); ++iter)
    {
        const char& c = *iter;
        unsigned f = static_cast<unsigned>(c - 'a');
        unsigned t = static_cast<unsigned>(mod(static_cast<int>(f) + b, 26));
        char d = static_cast<char>(t) + 'a';

        result.emplace_back(std::make_tuple(c, f, t, d));
    }

    return result;
}

// Affine cipher
std::list<std::tuple<char, unsigned, unsigned, char>> affinecipher(int a, int b, std::string const& str)
{
    std::list<std::tuple<char, unsigned, unsigned, char>> result;

    for (auto iter = str.begin(); iter != str.end(); ++iter)
    {
        const char& c = *iter;
        unsigned f = static_cast<unsigned>(c - 'a');
        unsigned t = static_cast<unsigned>(mod(a * static_cast<int>(f) + b, 26));
        char d = static_cast<char>(t) + 'a';

        result.emplace_back(std::make_tuple(c, f, t, d));
    }

    return result;
}

// Affine decipher
std::list<std::tuple<char, unsigned, unsigned, char>> affinedecipher(int a, int b, std::string const& str)
{
    std::list<std::tuple<char, unsigned, unsigned, char>> result;

    for (auto iter = str.begin(); iter != str.end(); ++iter)
    {
        const char& c = *iter;
        unsigned f = static_cast<unsigned>(c - 'a');
        int inv = multinv(a, 26);
        unsigned t = static_cast<unsigned>(mod(inv * static_cast<int>(f) + inv * -b, 26));
        char d = static_cast<char>(t) + 'a';

        result.emplace_back(std::make_tuple(c, f, t, d));
    }

    return result;
}

template <typename T>
std::list<std::tuple<std::string, T, T>> rsaencrypt(std::string const& str, std::list<T> const& N, T e, T blockSize)
{
    static std::map<char, T> const& lut = []()
    {
        static std::map<char, T> table;

        for (char c = 'a'; c <= 'z'; ++c)
            table.emplace(c, (c - 'a') + 11);

        for (char c = 'A'; c <= 'Z'; ++c)
            table.emplace(c, (c - 'A') + 37);

        for (char c = '0'; c <= '9'; ++c)
            table.emplace(c, (c - '0') + 63);

        table.emplace('.', 73);
        table.emplace(',', 74);
        table.emplace('!', 75);
        table.emplace('?', 76);
        table.emplace(':', 77);
        table.emplace(';', 78);
        table.emplace('=', 79);
        table.emplace('+', 80);
        table.emplace('-', 81);
        table.emplace('*', 82);
        table.emplace('/', 83);
        table.emplace('^', 84);
        table.emplace('\\', 85);
        table.emplace('@', 86);
        table.emplace('#', 87);
        table.emplace('&', 88);
        table.emplace('(', 89);
        table.emplace(')', 90);
        table.emplace('[', 91);
        table.emplace(']', 92);
        table.emplace('{', 93);
        table.emplace('}', 94);
        table.emplace('$', 95);
        table.emplace('%', 96);
        table.emplace('_', 97);
        table.emplace('\'', 98);
        table.emplace(' ', 99);

        return table;
    }();

    std::list<std::tuple<std::string, T, T>> result;

    T o;

    if (N.size() == 1)
        o = N.front();
    else
    {
        o = 1;

        for (auto& n : N)
            o *= n;
    }

    auto strIter = str.begin();
    for (T block = 0; block * blockSize < str.length(); ++block)
    {
        std::string substr;
        T M = 0, last = T(str.length()) - (block * blockSize);

        if (last > blockSize)
            last = blockSize;

        for (T i = 0; i < last; ++i)
        {
            auto& c = *(strIter++);
            auto it = lut.find(c);

            if (it != lut.end())
            {
                substr += c;
                M *= 100;
                M += it->second;
            }
        }

        result.emplace_back(std::make_tuple(substr, M, modexp(M, e, o)));
    }

    return result;
}

template <typename T>
std::list<std::tuple<std::string, T, T>> rsaencrypt(T const& data, std::list<T> const& N, T e)
{
    std::list<std::tuple<std::string, T, T>> result;

    T o;

    if (N.size() == 1)
        o = N.front();
    else
    {
        o = 1;

        for (auto& n : N)
            o *= n;
    }

    result.emplace_back(std::make_tuple(data.toString(), data, modexp(data, e, o)));

    return result;
}

template <typename T>
std::list<std::tuple<T, T, std::string>> rsadecrypt(std::list<T> const& data, std::list<T> const& N, T e)
{
    static std::map<T, char> const& lut = []()
    {
        static std::map<T, char> table;

        for (char c = 'a'; c <= 'z'; ++c)
            table.emplace((c - 'a') + 11, c);

        for (char c = 'A'; c <= 'Z'; ++c)
            table.emplace((c - 'A') + 37, c);

        for (char c = '0'; c <= '9'; ++c)
            table.emplace((c - '0') + 63, c);

        table.emplace(73, '.');
        table.emplace(74, ',');
        table.emplace(75, '!');
        table.emplace(76, '?');
        table.emplace(77, ':');
        table.emplace(78, ';');
        table.emplace(79, '=');
        table.emplace(80, '+');
        table.emplace(81, '-');
        table.emplace(82, '*');
        table.emplace(83, '/');
        table.emplace(84, '^');
        table.emplace(85, '\\');
        table.emplace(86, '@');
        table.emplace(87, '#');
        table.emplace(88, '&');
        table.emplace(89, '(');
        table.emplace(90, ')');
        table.emplace(91, '[');
        table.emplace(92, ']');
        table.emplace(93, '{');
        table.emplace(94, '}');
        table.emplace(95, '$');
        table.emplace(96, '%');
        table.emplace(97, '_');
        table.emplace(98, '\'');
        table.emplace(99, ' ');

        return table;
    }();

    std::list<std::tuple<T, T, std::string>> result;

    T o = 1, p = 1;
    
    if (N.size() == 1)
        p = phi(o = N.front());
    else
    {
        for (auto& n : N)
        {
            p *= n - 1;
            o *= n;
        }
    }

    T d = multinv(e, p);
    //std::cout << "d: " << d <<  ", p: " << p << ", o: " << o << std::endl;

    for (auto& M : data)
    {
        std::string str;
        T n = modexp(M, d, o);
        //std::cout << "M: " << M << ", n: " << n << std::endl;
        T num = n;

        while (num > 0)
        {
            auto it = lut.find(num % 100);
            num /= 100;

            if (it != lut.end())
                str = it->second + str;
        }

        result.emplace_back(std::make_tuple(M, n, str));
    }

    return result;
}

template <typename T>
std::list<std::tuple<std::string, T, T>> elgamalencrypt(std::string const& str, T const& p, T const& r, T const& ra, T const& b, T const& blockSize)
{
    static std::map<char, T> const& lut = []()
    {
        static std::map<char, T> table;

        for (char c = 'a'; c <= 'z'; ++c)
            table.emplace(c, (c - 'a') + 11);

        for (char c = 'A'; c <= 'Z'; ++c)
            table.emplace(c, (c - 'A') + 37);

        for (char c = '0'; c <= '9'; ++c)
            table.emplace(c, (c - '0') + 63);

        table.emplace('.', 73);
        table.emplace(',', 74);
        table.emplace('!', 75);
        table.emplace('?', 76);
        table.emplace(':', 77);
        table.emplace(';', 78);
        table.emplace('=', 79);
        table.emplace('+', 80);
        table.emplace('-', 81);
        table.emplace('*', 82);
        table.emplace('/', 83);
        table.emplace('^', 84);
        table.emplace('\\', 85);
        table.emplace('@', 86);
        table.emplace('#', 87);
        table.emplace('&', 88);
        table.emplace('(', 89);
        table.emplace(')', 90);
        table.emplace('[', 91);
        table.emplace(']', 92);
        table.emplace('{', 93);
        table.emplace('}', 94);
        table.emplace('$', 95);
        table.emplace('%', 96);
        table.emplace('_', 97);
        table.emplace('\'', 98);
        table.emplace(' ', 99);

        return table;
    }();

    std::list<std::tuple<std::string, T, T>> result;

    auto strIter = str.begin();
    for (T block = 0; block * blockSize < str.size(); ++block)
    {
        std::string substr;
        T M = 0, last = T(str.length()) - (block * blockSize);

        if (last > blockSize)
            last = blockSize;

        for (T i = 0; i < last; ++i)
        {
            auto& c = *(strIter++);
            auto it = lut.find(c);

            if (it != lut.end())
            {
                substr += c;
                M *= 100;
                M += it->second;
            }
        }

        result.emplace_back(substr, M, mod(M * modexp(ra, b, p), p));
    }

    return result;
}

template <typename T>
std::list<std::tuple<T, T, std::string>> elgamaldecrypt(std::list<T> const& data, T const& p, T const& rb, T const& a)
{
    static std::map<T, char> const& lut = []()
    {
        static std::map<T, char> table;

        for (char c = 'a'; c <= 'z'; ++c)
            table.emplace((c - 'a') + 11, c);

        for (char c = 'A'; c <= 'Z'; ++c)
            table.emplace((c - 'A') + 37, c);

        for (char c = '0'; c <= '9'; ++c)
            table.emplace((c - '0') + 63, c);

        table.emplace(73, '.');
        table.emplace(74, ',');
        table.emplace(75, '!');
        table.emplace(76, '?');
        table.emplace(77, ':');
        table.emplace(78, ';');
        table.emplace(79, '=');
        table.emplace(80, '+');
        table.emplace(81, '-');
        table.emplace(82, '*');
        table.emplace(83, '/');
        table.emplace(84, '^');
        table.emplace(85, '\\');
        table.emplace(86, '@');
        table.emplace(87, '#');
        table.emplace(88, '&');
        table.emplace(89, '(');
        table.emplace(90, ')');
        table.emplace(91, '[');
        table.emplace(92, ']');
        table.emplace(93, '{');
        table.emplace(94, '}');
        table.emplace(95, '$');
        table.emplace(96, '%');
        table.emplace(97, '_');
        table.emplace(98, '\'');
        table.emplace(99, ' ');

        return table;
    }();

    std::list<std::tuple<T, T, std::string>> result;

    for (auto& M : data)
    {
        std::string str;
        T n = mod(M * multinv(modexp(rb, a, p), p), p);
        T num = n;

        while (num > 0)
        {
            auto it = lut.find(num % 100);
            num /= 100;

            if (it != lut.end())
                str = it->second + str;
        }

        result.emplace_back(M, n, str);
    }

    return result;
}

// Print functions

template <typename T>
void euler_print(T a, T b)
{
	auto mat = euler(a, b);

	for (auto& e : mat)
		std::cout << e[0] << " " << e[1] << " " << e[2] << " " << e[3] << " " << e[4] << std::endl;

	std::cout << std::endl;

	for (auto it = mat.begin(), end = (++mat.rbegin()).base(); it != end; ++it)
		std::cout << (*it)[0] << " = " << (*it)[1] << " * " << (*it)[2] << " + " << (*it)[3] << std::endl;

	auto& last = *mat.rbegin();
	std::cout << last[0] << " * " << last[1] << " + " << last[2] << " * " << last[3] << " = " << last[4] << std::endl;
}

template <typename T>
void sieve_print(T a, T b)
{
    auto primes = sieve(a, b);

    T index = 0;
    for (auto iter = primes.begin(), last = primes.end(); iter != last; ++iter, ++index)
    {
        std::cout << *iter;

        if (std::next(iter) != last)
        {
            if (index > 0 && (index + 1) % 10 == 0)
                std::cout << std::endl;
            else std::cout << " ";
        }
    }

    std::cout << std::endl;
}

template <typename T>
void trialdivision_print(T num)
{
    auto factors = trialdivision(num);
    
    for (auto const& factor : factors)
        std::cout << "p = " << factor.first << ", e = " << factor.second << std::endl;
}

template <typename T>
void lineareqsolver_print(T a, T b, T c)
{
    auto result = lineareqsolver(a, b, c);

    std::cout << "x = " << result.first.first << (result.first.second >= 0 ? std::string(" + ") + std::to_string(result.first.second) + "t" : std::string(" - ") + std::to_string(-result.first.second) + "t") << std::endl;
    std::cout << "y = " << result.second.first << (result.second.second >= 0 ? std::string(" + ") + std::to_string(result.second.second) + "t" : std::string(" - ") + std::to_string(-result.second.second) + "t") << std::endl;
}

template <typename T>
void multinv_print(T a, T m)
{
    T inv = multinv(a, m);

    std::cout << inv << std::endl;
}

// TODO: Conjecture solver printer

template <typename T>
void modexp_print(T a, T x, T m)
{
    T exp = modexp(a, x, m);

    std::cout << exp << std::endl;
}

template <typename T>
void chinrem_print(std::vector<std::pair<T, T>> const& m)
{
    auto rem = chinrem(std::list<std::pair<T, T>>(m.begin(), m.end()));

    std::cout << rem.first << " " << rem.second << std::endl;
}

template <typename T>
void wilsonsprimetest_print(T n)
{
    std::cout << (wilsonsprimetest(n) ? "This is a prime" : "This is not a prime") << std::endl;
}

void cipher_print(std::list<std::tuple<char, unsigned, unsigned, char>> const& cipher)
{
    bool first = true;
    for (auto& c : cipher)
    {
        std::cout << (!first ? " " : "") << std::get<0>(c);
        first = false;
    }
    std::cout << std::endl;

    first = true;
    for (auto& c : cipher)
    {
        std::cout << (!first ? " " : "") << std::get<1>(c);
        first = false;
    }
    std::cout << std::endl;

    first = true;
    for (auto& c : cipher)
    {
        std::cout << (!first ? " " : "") << std::get<2>(c);
        first = false;
    }
    std::cout << std::endl;

    first = true;
    for (auto& c : cipher)
    {
        std::cout << (!first ? " " : "") << std::get<3>(c);
        first = false;
    }
    std::cout << std::endl;
}

void ceasar_print(int b, std::string const& str)
{
    cipher_print(ceasar(b, str));
}

void affinecipher_print(int a, int b, std::string const& str)
{
    cipher_print(affinecipher(a, b, str));
}

void affinedecipher_print(int a, int b, std::string const& str)
{
    cipher_print(affinedecipher(a, b, str));
}

template <typename T>
void phi_print(T n)
{
    std::cout << phi(n) << std::endl;
}

template <typename T>
void rsaencrypt_print(std::string const& str, std::list<T> const& N, T e, T blockSize)
{
    auto rsa = rsaencrypt(str, N, e, blockSize);

    for (auto& block : rsa)
        std::cout << "\"" << std::get<0>(block) << "\"  " << std::get<1>(block) << "  " << std::get<2>(block) << std::endl;
}

template <typename T>
void rsaencrypt_print(T const& data, std::list<T> const& N, T e)
{
    auto rsa = rsaencrypt(data, N, e);

    for (auto& block : rsa)
        std::cout << "\"" << std::get<0>(block) << "\"  " << std::get<1>(block) << "  " << std::get<2>(block) << std::endl;
}

template <typename T>
void rsadecrypt_print(std::list<T> const& data, std::list<T> const& N, T e)
{
    auto rsa = rsadecrypt(data, N, e);

    for (auto& block : rsa)
        std::cout << std::get<0>(block) << "  " << std::get<1>(block) << "  \"" << std::get<2>(block) << "\"" << std::endl;
}

template <typename T>
void primroots_print(T const& m, T const& c)
{
    auto roots = primroots(m, c);

    std::cout << "Count: " << phi(phi(m)) << " (" << roots.size() << ")" << std::endl;
    if (!roots.empty())
    {
        bool first = true;
        for (auto& root : roots)
        {
            if (!first)
                std::cout << ", ";
            else (!(first = false));

            std::cout << root;
        }
    }
    else std::cout << "None";

    std::cout << std::endl;
}

template <typename T>
void divisors_print(T const& m)
{
    auto divisors = getDivisors(m);

    std::cout << "Count: " << divisors.size() << std::endl;
    bool first = true;
    for (auto& divisor : divisors)
    {
        if (!first)
            std::cout << ", ";
        else (!(first = false));

        std::cout << divisor;
    }

    std::cout << std::endl;
}

template <typename T>
void elgamalencrypt_print(std::string const& str, T const& p, T const& r, T const& ra, T const& b, T const& blockSize)
{
    auto elgamal = elgamalencrypt(str, p, r, ra, b, blockSize);

    std::cout << "_  _  " << modexp(r, b, p) << std::endl;

    for (auto& block : elgamal)
        std::cout << "\"" << std::get<0>(block) << "\"  " << std::get<1>(block) << "  " << std::get<2>(block) << std::endl;
}

template <typename T>
void elgamaldecrypt_print(std::list<T> const& data, T const& p, T const& rb, T const& a)
{
    auto elgamal = elgamaldecrypt(data, p, rb, a);

    for (auto& block : elgamal)
        std::cout << std::get<0>(block) << "  " << std::get<1>(block) << "  \"" << std::get<2>(block) << "\"" << std::endl;
}

template <typename T>
void fermatfactor_print(T num)
{
    auto factors = fermatfactor(num);

    for (auto const& factor : factors)
        std::cout << "b = " << factor.first << ", e = " << factor.second << std::endl;
}

template <typename T>
void pollardrho_print(T const& N, std::string const& func, T const& x0)
{
    auto factors = pollardrho(N, func, x0);

    std::cout << "steps:         " << factors[0] << std::endl;
    std::cout << "xi:            " << factors[1] << std::endl;
    std::cout << "yi:            " << factors[2] << std::endl;
    std::cout << "gcd(xi-yi, N): " << factors[3] << std::endl;
    std::cout << "N/gcd:         " << factors[4] << std::endl;
}

template <typename T>
void pollardp_1_print(T const& N, T const& a, T const& M)
{
    auto factors = pollardp_1(N, a, M);

    std::cout << "a:                " << factors[0] << std::endl;
    std::cout << "m:                " << factors[1] << std::endl;
    std::cout << "N:                " << factors[2] << std::endl;
    std::cout << "gcd(a^(m!)-1, N): " << factors[3] << std::endl;
    std::cout << "N/gcd:            " << factors[4] << std::endl;
}

template <typename T>
void continuedfractionsqrt_print(T const& N, T const& depth)
{
    auto continuedfraction = continuedfractionsqrt(N, depth);

    T n = 0;
    for (auto& entry : continuedfraction)
        std::cout << "n:  " << n++ << ",  A_n:  " << std::get<0>(entry) << ",  B_n:  " << std::get<1>(entry) << ",  a_n:  " << std::get<2>(entry) << std::endl;
}

template <typename T>
void continuedfractionfactor_print(T const& N, std::list<T> const& base, T const& depth)
{
    auto factor = continuedfractionfactor(N, base, depth);

    for (auto& entry : factor)
    {
        std::cout << "n = " << std::get<0>(entry) << ", p_n = " << std::get<1>(entry) << ", b_n+1 = " << std::get<2>(entry) << ", " << std::get<3>(entry);
        auto& factors = std::get<4>(entry);
        for (auto& f : factors)
            std::cout << " " << f.first << "^" << f.second;
        std::cout << std::endl;
    }
}

auto& getVariables()
{
    static std::map<std::string, std::string, std::function<bool(std::string const&, std::string const&)>> variables([](std::string const& lhs, std::string const& rhs) -> bool {
        return lhs.length() > rhs.length() || (lhs.length() == rhs.length() && lhs.compare(rhs));
    });

    return variables;
}

std::string evaluate(const std::string& str)
{
    static std::unordered_map<std::string, std::string(*)(std::vector<std::string>)> evalFuncs = []()
    {
        std::unordered_map<std::string, std::string(*)(std::vector<std::string>)> funcs;

        funcs.emplace("babygiant", [](std::vector<std::string> params) -> std::string {
            if (params.size() >= 3)
            {
                BigInt a = params[0], p = params[1], m = params[2];
                return std::to_string(babygiant(a, p, m));
            }

            std::cerr << "Expression malformed: Too few parameters passed to function \"babygiant\"." << std::endl;
            throw std::runtime_error("");
        });

        funcs.emplace("factors", [](std::vector<std::string> params) -> std::string {
            if (params.size() >= 2)
            {
                BigInt a = params[0], b = params[1];
                return std::to_string(factors(a, b));
            }

            std::cerr << "Expression malformed: Too few parameters passed to function \"factors\"." << std::endl;
            throw std::runtime_error("");
        });

        funcs.emplace("gcd", [](std::vector<std::string> params) -> std::string {
            if (params.size() >= 2)
            {
                BigInt a = params[0], b = params[1];
                return std::to_string(euler_lite(a, b));
            }

            std::cerr << "Expression malformed: Too few parameters passed to function \"gcd\"." << std::endl;
            throw std::runtime_error("");
        });

        funcs.emplace("index", [](std::vector<std::string> params) -> std::string {
            if (params.size() >= 3)
            {
                BigInt a = params[0], p = params[1], m = params[2];
                return std::to_string(index(a, p, m));
            }

            std::cerr << "Expression malformed: Too few parameters passed to function \"index\"." << std::endl;
            throw std::runtime_error("");
        });

        funcs.emplace("mod", [](std::vector<std::string> params) -> std::string {
            if (params.size() >= 2)
            {
                BigInt a = params[0], b = params[1];
                return std::to_string(mod(a, b));
            }

            std::cerr << "Expression malformed: Too few parameters passed to function \"mod\"." << std::endl;
            throw std::runtime_error("");
        });

        funcs.emplace("modexp", [](std::vector<std::string> params) -> std::string {
            if (params.size() >= 3)
            {
                BigInt a = params[0], x = params[1], m = params[2];
                return std::to_string(modexp(a, x, m));
            }

            std::cerr << "Expression malformed: Too few parameters passed to function \"modexp\"." << std::endl;
            throw std::runtime_error("");
        });

        funcs.emplace("multinv", [](std::vector<std::string> params) -> std::string {
            if (params.size() >= 2)
            {
                BigInt a = params[0], m = params[1];
                return std::to_string(multinv(a, m));
            }

            std::cerr << "Expression malformed: Too few parameters passed to function \"multinv\"." << std::endl;
            throw std::runtime_error("");
        });

        funcs.emplace("nthroot", [](std::vector<std::string> params) -> std::string {
            if (params.size() >= 2)
            {
                BigInt a = params[0], N = params[1];
                return std::to_string(std::nthrooti(a, N));
            }

            std::cerr << "Expression malformed: Too few parameters passed to function \"nthroot\"." << std::endl;
            throw std::runtime_error("");
        });

        funcs.emplace("order", [](std::vector<std::string> params) -> std::string {
            if (params.size() >= 2)
            {
                BigInt a = params[0], m = params[1];
                return std::to_string(order(a, m));
            }

            std::cerr << "Expression malformed: Too few parameters passed to function \"order\"." << std::endl;
            throw std::runtime_error("");
        });

        funcs.emplace("phi", [](std::vector<std::string> params) -> std::string {
            if (params.size() >= 1)
            {
                BigInt a = params[0];
                return std::to_string(phi(a));
            }

            std::cerr << "Expression malformed: Too few parameters passed to function \"phi\"." << std::endl;
            throw std::runtime_error("");
        });

        return funcs;
    }();

    std::string expr = str;

    size_t i = 0;

    // Strip leading and trailing whitespace
    size_t ls = 0, ts = expr.size() - 1;
    while (expr[ls] == ' ')
        ++ls;
    while (expr[ts] == ' ')
        --ts;
    if (ls != 0 || ts != expr.size() - 1)
        expr = expr.substr(ls, (ts - ls) + 1);

    if (expr.empty())
    {
        std::cerr << "Expression malformed: Unexpected empty token." << std::endl;
        throw std::runtime_error("");
    }

    std::list<std::tuple<std::string, size_t, size_t>> funcCalls;

    // Parenthesis
    while ((i = expr.find('(', i)) != expr.npos)
    {
        size_t j = i;
        for (size_t nextOpen = expr.find('(', i + 1), nextClose = expr.find(')', i + 1); nextOpen != expr.npos || nextClose != expr.npos;)
        {
            if (nextClose < nextOpen)
            {
                j = nextClose;
                break;
            }

            if (nextOpen != expr.npos)
                nextOpen = expr.find('(', nextOpen + 1);
            if (nextClose != expr.npos)
                nextClose = expr.find(')', nextClose + 1);
        }

        if (j == i)
        {
            std::cerr << "Expression malformed: Mismatched parenthesis." << std::endl;
            throw std::runtime_error("");
        }

        std::string prefix = expr.substr(0, i);

        size_t k = i;
        for (; k > 0 && expr[k - 1] == ' '; --k);

        if (k != 0)
        {
            bool skip = false;
            for (auto& funcPair : evalFuncs)
            {
                auto& func = funcPair.first;

                // Parenthesis associated with function
                if (func.size() <= k && prefix.substr(k - func.size(), func.size()) == func)
                {
                    funcCalls.emplace_back(func, k - func.size(), j);
                    skip = true;
                    break;
                }
            }

            if (skip)
            {
                i = j + 1;
                continue;
            }

            if (expr[k - 1] >= '0' && expr[k - 1] <= '9')
                prefix += "*";
        }

        std::string suffix = expr.substr(j + 1);

        size_t l = 0;
        for (size_t last = suffix.size(); l < last && suffix[l] == ' '; ++l);

        if (l != suffix.size() && ((suffix[l] >= '0' && suffix[l] <= '9') || (suffix[l] != '+' && suffix[l] != '-' && suffix[l] != '*' && suffix[l] != '/' && suffix[l] != '^')))
            suffix = "*" + suffix;

        expr = prefix + evaluate(expr.substr(i + 1, ((j - 1) - (i + 1)) + 1)) + suffix;
    }

    // Evaluate functions
    for (auto& funcCall : funcCalls)
    {
        auto& func = std::get<0>(funcCall);
        auto& fromIndex = std::get<1>(funcCall);
        auto& toIndex = std::get<2>(funcCall);

        size_t from = fromIndex + func.size() + 1;

        std::vector<std::string> params;

        if (from != toIndex)
        {
            std::string paramStr = expr.substr(from, toIndex - from);
            size_t scopeLevel = 0, f = 0, g = 0;
            bool inString = false;

            for (size_t k = paramStr.find(','); k != paramStr.npos; k = paramStr.find(',', k + 1))
            {
                for (; g < k; ++g)
                {
                    if (paramStr[g] == '"' && (g == 0 || paramStr[g - 1] != '\\'))
                        inString = !inString;
                    else if (!inString && paramStr[g] == '(')
                        ++scopeLevel;
                    else if (!inString && paramStr[g] == ')')
                        --scopeLevel;
                }

                if (!scopeLevel && !inString)
                {
                    std::string par = paramStr.substr(f, k - f);
                    bool isStr = par.front() == '"';
                    params.emplace_back(!isStr ? evaluate(par) : par.substr(1, par.size() - 2));
                    f = k + 1;
                    for (; f < paramStr.length() && paramStr[f] == ' '; ++f);
                }
            }

            std::string par = paramStr.substr(f);
            bool isStr = par.front() == '"';

            if (inString || (isStr && par.back() != '"'))
            {
                std::cerr << "Expression malformed: Missing quotation mark(s)." << std::endl;
                throw std::runtime_error("");
            }
            
            params.emplace_back(!isStr ? evaluate(par) : par.substr(1, par.size() - 2));
        }

        std::string prefix = expr.substr(0, fromIndex);

        size_t k = fromIndex;
        for (; k > 0 && expr[k - 1] == ' '; --k);

        if (k != 0 && expr[k - 1] >= '0' && expr[k - 1] <= '9')
            prefix += "*";

        std::string suffix = expr.substr(toIndex + 1);

        size_t l = 0;
        for (size_t last = suffix.size(); l < last && suffix[l] == ' '; ++l);

        if (l != suffix.size() && ((suffix[l] >= '0' && suffix[l] <= '9') || (suffix[l] != '+' && suffix[l] != '-' && suffix[l] != '*' && suffix[l] != '/' && suffix[l] != '^')))
            suffix = "*" + suffix;

        expr = prefix + (evalFuncs.find(func)->second)(params) + suffix;
    }

    // Define Variable rhs
    if ((i = expr.rfind("=:")) != expr.npos)
    {
        BigInt lhs = evaluate(expr.substr(0, i));
        std::string rhs = expr.substr(i + 2);

        while (!rhs.empty() && rhs.front() == ' ')
            rhs = rhs.substr(1);

        while (!rhs.empty() && rhs.back() == ' ')
            rhs = rhs.substr(0, rhs.size() - 1);

        if (rhs.empty())
        {
            std::cerr << "Expression malformed: A variable name was expected, but none was given." << std::endl;
            throw std::runtime_error("");
        }

        for (auto& c : rhs)
        {
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
            {
                std::cerr << "Expression malformed: A variable name can only contain alphabetical characters." << std::endl;
                throw std::runtime_error("");
            }
        }

        getVariables()[rhs] = expr = std::to_string(lhs);
    }

    // Define Variable lhs
    if ((i = expr.rfind(":=")) != expr.npos)
    {
        std::string lhs = expr.substr(0, i);
        BigInt rhs = evaluate(expr.substr(i + 2));

        while (!lhs.empty() && lhs.front() == ' ')
            lhs = lhs.substr(1);

        while (!lhs.empty() && lhs.back() == ' ')
            lhs = lhs.substr(0, lhs.size() - 1);

        if (lhs.empty())
        {
            std::cerr << "Expression malformed: A variable name was expected, but none was given." << std::endl;
            throw std::runtime_error("");
        }

        for (auto& c : lhs)
        {
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
            {
                std::cerr << "Expression malformed: A variable name can only contain alphabetical characters." << std::endl;
                throw std::runtime_error("");
            }
        }

        getVariables()[lhs] = expr = std::to_string(rhs);
    }

    // Replace known variables
    for (auto iter = getVariables().begin(), last = getVariables().end(); iter != last; ++iter)
    {
        std::string const& var = iter->first;
        std::string const& value = iter->second;

        while ((i = expr.rfind(var)) != expr.npos)
        {
            std::string prefix = expr.substr(0, i);
            std::string suffix = expr.substr(i + var.length());

            for (i = prefix.size(); i > 0 && prefix[i - 1] == ' '; --i);

            if (i > 0 && ((prefix[i - 1] >= '0' && prefix[i - 1] <= '9') || (prefix[i - 1] >= 'a' && prefix[i - 1] <= 'z') || (prefix[i - 1] >= 'A' && prefix[i - 1] <= 'Z') || prefix[i - 1] == ')'))
                prefix += '*';

            for (i = 0; i < suffix.size() && suffix[i] == ' '; ++i);

            if (i < suffix.size() && ((suffix[i] >= '0' && suffix[i] <= '9') || (suffix[i] >= 'a' && suffix[i] <= 'z') || (suffix[i] >= 'A' && suffix[i] <= 'Z') || suffix[i] == '('))
                suffix = '*' + suffix;

            expr = prefix + value + suffix;
        }
    }

    // Addition
    if ((i = expr.rfind('+')) != expr.npos)
    {
        BigInt lhs = evaluate(expr.substr(0, i));
        BigInt rhs = evaluate(expr.substr(i + 1));

        expr = std::to_string(lhs + rhs);
    }

    // Subtraction
    for (i = expr.rfind('-'); i != expr.npos;)
    {
        // Negative number
        if (i == 0)
            break;

        size_t j = i;
        for (; j > 0 && expr[j - 1] == ' '; --j);

        if (expr[j - 1] < '0' || expr[j - 1] > '9')
        {
            if (expr[j - 1] == '-')
                i = j - 1;
            else
                i = expr.rfind('-', j - 1);
            continue;
        }

        BigInt lhs = evaluate(expr.substr(0, i));
        BigInt rhs = evaluate(expr.substr(i + 1));

        expr = std::to_string(lhs - rhs);
        break;
    }

    // Division
    if ((i = expr.rfind('/')) != expr.npos)
    {
        BigInt lhs = evaluate(expr.substr(0, i));
        BigInt rhs = evaluate(expr.substr(i + 1));

        expr = std::to_string(lhs / rhs);
    }

    // Multiplication
    if ((i = expr.rfind('*')) != expr.npos)
    {
        BigInt lhs = evaluate(expr.substr(0, i));
        BigInt rhs = evaluate(expr.substr(i + 1));

        expr = std::to_string(lhs * rhs);
    }

    // Exponents
    if ((i = expr.rfind('^')) != expr.npos)
    {
        BigInt lhs = evaluate(expr.substr(0, i));
        BigInt rhs = evaluate(expr.substr(i + 1));

        expr = std::to_string(std::powi(lhs, rhs));
    }

    return expr;
}

int main()
{
    if (!SetConsoleCtrlHandler(signalHandler, TRUE))
        std::cerr << "Failed to set control handler." << std::endl;

    while (running)
    {
        if (funcThread.joinable())
            funcThread.join();

        if (running)
        {
            funcThread = std::thread([]()
                {
                    std::string func;
                    std::cin >> func;

                    calculating = true;

                    if (!func.empty())
                        std::cout << std::endl;

                    try
                    {
                        if (func == "exit")
                            exit(0);
                        else if (func == "mod")
                        {
                            BigInt a, b;
                            std::cin >> a >> b;

                            std::cout << mod(a, b) << std::endl;
                        }
                        else if (func == "euler" || func == "gcd")   // euler a b
                        {
                            BigInt a, b;
                            std::cin >> a >> b;

                            euler_print(a, b);
                        }
                        else if (func == "sieve")   // sieve lower higher
                        {
                            BigInt a, b;
                            std::cin >> a >> b;

                            sieve_print(a, b);
                        }
                        else if (func == "trialdivision" || func == "factor")   // trialdivision num
                        {
                            BigInt a;
                            std::cin >> a;

                            trialdivision_print(a);
                        }
                        else if (func == "lineareqsolver")  // lineareqsolver a b c
                        {
                            BigInt a, b, c;
                            std::cin >> a >> b >> c;

                            lineareqsolver_print(a, b, c);
                        }
                        else if (func == "multinv") // multinv a m
                        {
                            BigInt a, m;
                            std::cin >> a >> m;

                            multinv_print(a, m);
                        }
                        else if (func == "modexp")  // modexp a x m
                        {
                            BigInt a, x, m;
                            std::cin >> a >> x >> m;

                            modexp_print(a, x, m);
                        }
                        else if (func == "chinrem") // chinrem [number pairs]
                        {
                            std::vector<std::pair<BigInt, BigInt>> input;

                            std::string line;
                            std::getline(std::cin, line);
                            std::istringstream str(line);

                            BigInt a, b;
                            while (str >> a && str >> b)
                                input.push_back(std::make_pair(a, b));

                            chinrem_print(input);
                        }
                        else if (func == "wilsonsprimetest" || func == "isprime" || func == "test")    // wilsonsprimetest n
                        {
                            BigInt a;
                            std::cin >> a;

                            wilsonsprimetest_print(a);
                        }
                        else if (func == "factors") // factors a b
                        {
                            BigInt a, b;
                            std::cin >> a >> b;

                            std::cout << factors(a, b) << std::endl;
                        }
                        else if (func == "ceasar")  // ceasar b str
                        {
                            int b;
                            std::string str;
                            std::cin >> b >> str;

                            ceasar_print(b, str);
                        }
                        else if (func == "affinecipher")    // affinecipher a b str
                        {
                            int a, b;
                            std::string str;
                            std::cin >> a >> b >> str;

                            affinecipher_print(a, b, str);
                        }
                        else if (func == "affinedecipher")  // affinedecipher a b str
                        {
                            int a, b;
                            std::string str;
                            std::cin >> a >> b >> str;

                            affinedecipher_print(a, b, str);

                        }
                        else if (func == "affinedecipherbf")  // affinedecipherbf str [a table]
                        {
                            std::string str;
                            std::cin >> str;

                            std::string line;
                            std::getline(std::cin, line);
                            std::istringstream stream(line);

                            std::list<int> al;
                            int num;
                            while (stream >> num)
                                al.push_back(num);

                            if (al.empty())
                                al = sieve(2, 25);

                            for (auto& a : al)
                            {
                                for (int b = 0; b < 26; ++b)
                                {
                                    std::cout << "a = " << a << ", b = " << b << std::endl;
                                    try
                                    {
                                        affinedecipher_print(a, b, str);
                                    }
                                    catch (...) {}
                                    std::cout << std::endl;
                                }
                            }
                        }
                        else if (func == "phi") // phi N
                        {
                            BigInt n;
                            std::cin >> n;

                            phi_print(n);
                        }
                        else if (func == "phisearch")   // phisearch N from to
                        {
                            BigInt n, from, to;
                            std::cin >> n >> from >> to;

                            for (BigInt i = from; i <= to; ++i)
                            {
                                if (phi(i) == n)
                                    std::cout << "Found at phi(" << i << ")." << std::endl;
                            }

                            std::cout << "Search complete." << std::endl;
                        }
                        else if (func == "rsaencrypt")  // rsaencrypt str modulus exponent blockSize
                        {
                            std::string str;
                            bool isString = false;

                            do
                            {
                                std::string t;
                                std::cin >> t;

                                if (!str.empty())
                                    str += " ";
                                str += t;
                            } while (str.front() == '"' && (str.back() != '"' || str.length() == 1));

                            if (str.front() == '"')
                            {
                                str = str.substr(1, str.length() - 2);
                                isString = true;
                            }
                            else
                            {
                                for (auto const& c : str)
                                {
                                    if (!((c >= '0' && c <= '9') || c == '-'))
                                    {
                                        isString = true;
                                        break;
                                    }
                                }
                            }

                            bool modRange = false;
                            std::list<BigInt> m;
                            std::string t;

                            while (std::cin >> t)
                            {
                                if (!modRange && t.front() == '[')
                                {
                                    m.emplace_back(t.substr(1));

                                    modRange = true;
                                }
                                else if (modRange && t.back() == ']')
                                {
                                    m.emplace_back(t.substr(0, t.length() - 1));
                                    break;
                                }
                                else
                                {
                                    m.emplace_back(t);
                                    break;
                                }
                            }

                            BigInt e, bs;
                            std::cin >> e;

                            if (isString)
                            {
                                std::cin >> bs;

                                rsaencrypt_print(str, m, e, bs);
                            }
                            else
                            {
                                BigInt data = str;
                                rsaencrypt_print(data, m, e);
                            }
                        }
                        else if (func == "rsadecrypt")  // rsadecrypt [data] modulus exponent
                        {
                            std::list<BigInt> data;
                            std::list<BigInt> modulus;
                            BigInt t;
                            std::string u;
                            bool modRange = false;

                            while (data.size() < (modulus.empty() ? 3 : 2))
                            {
                                std::string line;
                                std::getline(std::cin, line);
                                std::istringstream str(line);

                                while (str >> u)
                                {
                                    if (data.size())
                                    {
                                        if (!modRange && u[0] == '[')
                                        {
                                            u = u.substr(1);

                                            if (!u.empty())
                                                modulus.emplace_back(t = u);

                                            modRange = true;
                                        }
                                        else if (modRange && u.back() == ']')
                                        {
                                            u = u.substr(0, u.length() - 1);

                                            if (!u.empty())
                                                modulus.emplace_back(t = u);

                                            modRange = false;
                                        }
                                        else data.emplace_back(t = u);
                                    }
                                    else data.emplace_back(t = u);
                                }
                            }

                            BigInt e = data.back();
                            data.pop_back();

                            if (modulus.empty())
                            {
                                BigInt m = data.back();
                                data.pop_back();
                                modulus.emplace_back(m);
                            }

                            rsadecrypt_print(data, modulus, e);
                        }
                        else if (func == "order" || func == "ord")  // order a modulus
                        {
                            BigInt a, m;

                            std::cin >> a >> m;

                            std::cout << order(a, m) << std::endl;
                        }
                        else if (func == "primroots" || func == "roots") // primroots m [count]
                        {
                            long long m, c = -1;

                            std::string line;
                            std::getline(std::cin, line);
                            std::istringstream str(line);

                            str >> m >> c;

                            primroots_print(m, c);
                        }
                        else if (func == "index")   // index result base modulus
                        {
                            BigInt a, p, m;

                            std::cin >> a >> p >> m;

                            std::cout << index(a, p, m) << std::endl;
                        }
                        else if (func == "babygiant")   // babygiant result base modulus
                        {
                            BigInt a, p, m;

                            std::cin >> a >> p >> m;

                            std::cout << babygiant(a, p, m) << std::endl;
                        }
                        else if (func == "nthroot") // nthroot n num
                        {
                            BigInt n, num;

                            std::cin >> n >> num;

                            std::cout << std::nthrooti(num, n) << std::endl;
                        }
                        else if (func == "divisors") // divisors num
                        {
                            long long p;

                            std::cin >> p;

                            divisors_print(p);
                        }
                        else if (func == "eval")    // eval expression
                        {
                            std::string line;
                            std::getline(std::cin, line);

                            std::cout << evaluate(line) << std::endl;
                        }
                        else if (func == "set") // set variable value
                        {
                            std::string var, val;
                            std::cin >> var;
                            std::getline(std::cin, val);

                            std::string result = evaluate(var + " := " + val);
                            std::cout << "Variable \"" << var << "\" set to " << result << std::endl;
                        }
                        else if (func == "unset")   // unset variable
                        {
                            std::string var;
                            std::cin >> var;

                            if (getVariables().erase(var))
                                std::cout << "Variable \"" << var << "\" unset." << std::endl;
                            else
                                std::cout << "Variable \"" << var << "\" not found." << std::endl;
                        }
                        else if (func == "elgamalencrypt")  // elgamalencrypt data prime primitiveroot ra b blockSize
                        {
                            std::string str;

                            do
                            {
                                std::string t;
                                std::cin >> t;

                                if (!str.empty())
                                    str += " ";
                                str += t;
                            } while (str.front() == '"' && (str.back() != '"' || str.length() == 1));

                            if (str.front() == '"')
                                str = str.substr(1, str.length() - 2);

                            BigInt p, r, ra, b, bs;
                            std::cin >> p >> r >> ra >> b >> bs;

                            elgamalencrypt_print(str, p, r, ra, b, bs);
                        }
                        else if (func == "elgamaldecrypt")  // elgamaldecrypt [data] prime rb a
                        {
                            std::list<BigInt> data;
                            BigInt t;
                            std::string u;

                            while (data.size() < 3)
                            {
                                std::string line;
                                std::getline(std::cin, line);
                                std::istringstream str(line);

                                while (str >> u)
                                    data.emplace_back(t = u);
                            }

                            BigInt a = data.back();
                            data.pop_back();
                            BigInt rb = data.back();
                            data.pop_back();
                            BigInt p = data.back();
                            data.pop_back();

                            elgamaldecrypt_print(data, p, rb, a);
                        }
                        else if (func == "fermatfactor" || func == "fermat")   // fermatfactor num
                        {
                            BigInt a;
                            std::cin >> a;

                            fermatfactor_print(a);
                        }
                        else if (func == "issquare")    // issquare num
                        {
                            BigInt num;
                            std::cin >> num;

                            std::cout << (std::issquare(num) ? "true" : "false") << std::endl;
                        }
                        else if (func == "rabinmiller")    // rabinmiller num
                        {
                            BigInt num;
                            std::cin >> num;

                            std::cout << (rabinmiller(num) ? "N is a strong probable prime." : "N is composite.") << std::endl;
                        }
                        else if (func == "strongfermattest")    // strongfermattest N base
                        {
                            BigInt N, b;
                            std::cin >> N >> b;

                            auto test = strongfermattest(N, b);
                            std::cout << (std::get<0>(test) ? "N acts like a prime with respect to base b." : "N is a composite number.") << " (k = " << std::get<1>(test) << ", s = " << std::get<2>(test) << ")" << std::endl;
                        }
                        else if (func == "pollardrho")  // pollardrho N func x0
                        {
                            BigInt N, x0;
                            std::string func;

                            std::cin >> N;

                            do
                            {
                                std::string t;
                                std::cin >> t;

                                if (!func.empty())
                                    func += " ";
                                func += t;
                            } while (func.front() == '"' && (func.back() != '"' || func.length() == 1));

                            if (func.front() == '"')
                                func = func.substr(1, func.length() - 2);

                            std::cin >> x0;

                            pollardrho_print(N, func, x0);
                        }
                        else if (func == "pollardp_1")  // pollardp_1 N base maxSteps
                        {
                            BigInt N, a, M;
                            std::cin >> N >> a >> M;

                            pollardp_1_print(N, a, M);
                        }
                        else if (func == "contfracsqrt")    // contfracsqrt N depth
                        {
                            BigInt N, depth;
                            std::cin >> N >> depth;

                            continuedfractionsqrt_print(N, depth);
                        }
                        else if (func == "tonelli_shanks")  // tonelli_shanks a odd_prime
                        {
                            BigInt a, p;
                            std::cin >> a >> p;

                            std::cout << tonelli_shanks(a, p) << std::endl;
                        }
                        else if (func == "quadeqsolver")    // quadeqsolver a p
                        {
                            BigInt a, p;
                            std::cin >> a >> p;

                            std::cout << quadeqsolver(a, p) << std::endl;
                        }
                        else if (func == "quadres" || func == "qres")   // quadres a b
                        {
                            BigInt a, b;
                            std::cin >> a >> b;

                            std::cout << quadres(a, b) << std::endl;
                        }
                        else if (func == "lehmertest")  // lehmertest N
                        {
                            BigInt n;
                            std::cin >> n;

                            std::cout << n << (lehmertest(n) ? " is a prime." : " is not a prime.") << std::endl;
                        }
                        else if (func == "contfracfactor" || func == "cffactor")    // contfracfactor N depth base
                        {
                            std::string line;
                            std::getline(std::cin, line);
                            std::stringstream ss(line);
                            
                            BigInt n, depth, m;
                            ss >> n >> depth;

                            std::list<BigInt> base;

                            while (ss >> m)
                                base.emplace_back(m);

                            continuedfractionfactor_print(n, base, depth);
                        }
                        else if (!func.empty())
                        {
                            std::cout << "Unknown command \"" << func << "\"." << std::endl;
                            std::cin.clear();
                            std::cin.ignore(INT_MAX, '\n');
                        }
                        else
                        {
                            std::cerr << "Closing..." << std::endl;
                            calculating = false;
                            running = false;
                            return;
                        }
                    }
                    catch (...) {}

                    calculating = false;

                    std::cout << std::endl;
                });
        }
    }

    return 0;
}

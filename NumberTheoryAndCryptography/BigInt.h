// MIT License

// Copyright (c) 2020 William Chanrico

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

/*
* Title: BigInt
* Description: Big Integer class (coded with competitive programming problems in mind)
* Date: 09-October-2017
* Author: William Chanrico
*/

#include <assert.h>
#include "bits/stdc++.h"
#include <string>
using namespace std;

class BigInt {
public:
    int sign = 1;
    string s;

    BigInt() : s("")
    {
    }

    BigInt(string x)
    {
        *this = x;
    }

    BigInt(int x)
    {
        *this = to_string(x);
    }

    BigInt negative() const
    {
        BigInt x = *this;

        x.sign *= -1;

        return x;
    }

    BigInt& normalize(int newSign)
    {
        for (int a = static_cast<int>(s.size() - 1); a > 0 && s[a] == '0'; a--)
            s.erase(s.begin() + a);

        sign = (s.size() == 1 && s[0] == '0' ? 1 : newSign);

        return *this;
    }

    BigInt& operator=(string x)
    {
        int newSign = (x[0] == '-' ? -1 : 1);

        s = (newSign == -1 ? x.substr(1) : x);

        reverse(s.begin(), s.end());

        this->normalize(newSign);

        return *this;
    }

    BigInt& operator=(int x)
    {
        return operator=(to_string(x));
    }

    bool operator==(const BigInt& x) const
    {
        return (s == x.s && sign == x.sign);
    }

    bool operator!=(const BigInt& x) const
    {
        return (s != x.s || sign != x.sign);
    }

    bool operator<(const BigInt& x) const
    {
        if (sign != x.sign)
            return sign < x.sign;

        if (s.size() != x.s.size())
            return (sign == 1 ? s.size() < x.s.size() : s.size() > x.s.size());

        for (int a = static_cast<int>(s.size() - 1); a >= 0; a--)
            if (s[a] != x.s[a])
                return (sign == 1 ? s[a] < x.s[a] : s[a] > x.s[a]);

        return false;
    }

    bool operator<=(const BigInt& x) const
    {
        return (*this < x || *this == x);
    }

    bool operator>(const BigInt& x) const
    {
        return (!(*this < x) && !(*this == x));
    }

    bool operator>=(const BigInt& x) const
    {
        return (*this > x || *this == x);
    }

    BigInt operator+(BigInt x) const
    {
        BigInt curr = *this;

        if (curr.sign != x.sign)
            return curr - x.negative();

        BigInt res;

        for (int a = 0, carry = 0; a < s.size() || a < x.s.size() || carry; a++) {
            carry += (a < curr.s.size() ? curr.s[a] - '0' : 0) + (a < x.s.size() ? x.s[a] - '0' : 0);

            res.s += (carry % 10 + '0');

            carry /= 10;
        }

        return res.normalize(sign);
    }

    BigInt& operator+=(BigInt x)
    {
        if (sign != x.sign)
            return operator-=(x.negative());

        std::string res;

        for (int a = 0, carry = 0; a < s.size() || a < x.s.size() || carry; a++) {
            carry += (a < s.size() ? s[a] - '0' : 0) + (a < x.s.size() ? x.s[a] - '0' : 0);

            res += (carry % 10 + '0');

            carry /= 10;
        }

        s = res;
        normalize(sign);

        return *this;
    }

    BigInt operator-(BigInt x) const
    {
        BigInt curr = *this;

        if (curr.sign != x.sign)
            return curr + x.negative();

        int realSign = curr.sign;

        curr.sign = x.sign = 1;

        if (curr < x)
            return ((x - curr).negative()).normalize(-realSign);

        BigInt res;

        for (int a = 0, borrow = 0; a < s.size(); a++) {
            borrow = (curr.s[a] - borrow - (a < x.s.size() ? x.s[a] : '0'));

            res.s += (borrow >= 0 ? borrow + '0' : borrow + '0' + 10);

            borrow = (borrow >= 0 ? 0 : 1);
        }

        return res.normalize(realSign);
    }

    BigInt operator-() const
    {
        return negative();
    }

    BigInt& operator-=(BigInt x)
    {
        if (sign != x.sign)
            return operator+=(x.negative());

        int realSign = sign;

        sign = x.sign = 1;

        if (*this < x)
        {
            operator=((x - *this).negative());
            normalize(-realSign);
            return *this;
        }

        std::string res;

        for (int a = 0, borrow = 0; a < s.size(); a++) {
            borrow = (s[a] - borrow - (a < x.s.size() ? x.s[a] : '0'));

            res += (borrow >= 0 ? borrow + '0' : borrow + '0' + 10);

            borrow = (borrow >= 0 ? 0 : 1);
        }

        s = res;
        normalize(realSign);

        return *this;
    }

    BigInt operator*(BigInt x) const
    {
        BigInt res("0");

        for (int a = 0, b = s[a] - '0'; a < s.size(); a++, b = s[a] - '0') {
            while (b--)
                res = (res + x);

            x.s.insert(x.s.begin(), '0');
        }

        return res.normalize(sign * x.sign);
    }

    BigInt& operator*=(BigInt x)
    {
        BigInt res("0");

        for (int a = 0, b = s[a] - '0'; a < s.size(); a++, b = s[a] - '0') {
            while (b--)
                res = (res + x);

            x.s.insert(x.s.begin(), '0');
        }

        s = res.s;
        normalize(sign * x.sign);

        return *this;
    }

    BigInt operator/(BigInt x) const
    {
        if (x.s.size() == 1 && x.s[0] == '0')
            x.s[0] /= (x.s[0] - '0');

        BigInt temp("0"), res;

        for (int a = 0; a < s.size(); a++)
            res.s += "0";

        int newSign = sign * x.sign;

        x.sign = 1;

        for (int a = static_cast<int>(s.size() - 1); a >= 0; a--) {
            temp.s.insert(temp.s.begin(), '0');
            temp = temp + s.substr(a, 1);

            while (!(temp < x)) {
                temp = temp - x;
                res.s[a]++;
            }
        }

        return res.normalize(newSign);
    }

    BigInt& operator/=(BigInt x)
    {
        if (x.s.size() == 1 && x.s[0] == '0')
            x.s[0] /= (x.s[0] - '0');

        BigInt temp("0"), res;

        for (int a = 0; a < s.size(); a++)
            res.s += "0";

        int newSign = sign * x.sign;

        x.sign = 1;

        for (int a = static_cast<int>(s.size() - 1); a >= 0; a--) {
            temp.s.insert(temp.s.begin(), '0');
            temp = temp + s.substr(a, 1);

            while (!(temp < x)) {
                temp = temp - x;
                res.s[a]++;
            }
        }

        s = res.s;
        normalize(newSign);

        return *this;
    }

    BigInt operator%(BigInt x) const
    {
        if (x.s.size() == 1 && x.s[0] == '0')
            x.s[0] /= (x.s[0] - '0');

        BigInt res("0");

        x.sign = 1;

        for (int a = static_cast<int>(s.size() - 1); a >= 0; a--) {
            res.s.insert(res.s.begin(), '0');

            res = res + s.substr(a, 1);

            while (!(res < x))
                res = res - x;
        }

        return res.normalize(sign);
    }

    BigInt& operator%=(BigInt x)
    {
        if (x.s.size() == 1 && x.s[0] == '0')
            x.s[0] /= (x.s[0] - '0');

        BigInt res("0");

        x.sign = 1;

        for (int a = static_cast<int>(s.size() - 1); a >= 0; a--) {
            res.s.insert(res.s.begin(), '0');

            res = res + s.substr(a, 1);

            while (!(res < x))
                res = res - x;
        }

        s = res.s;
        normalize(sign);

        return *this;
    }

    BigInt operator&(BigInt x) const
    {
        if (x.s.size() == 1 && x.s[0] == '0')
            return BigInt("0");

        std::string lhs = convertToBase(2), rhs = x.convertToBase(2);

        int l = std::min(lhs.size(), rhs.size());
        std::string res(l, '0');

        for (int i = 0; i < l; ++i)
            res[i] = '0' + ((s[i] - '0') & (x.s[i] - '0'));

        BigInt result(res);
        result = result.toBase10(2);

        return result;
    }

    BigInt& operator++()
    {
        operator+=(1);
        return *this;
    }

    BigInt operator++(int)
    {
        BigInt res = *this;
        operator+=(1);
        return res;
    }

    BigInt& operator--()
    {
        operator-=(1);
        return *this;
    }

    BigInt operator--(int)
    {
        BigInt res = *this;
        operator-=(1);
        return res;
    }

    // operator string(){
    // 	string ret = s;

    // 	reverse(ret.begin(), ret.end());

    // 	return (sign == -1 ? "-" : "") + s;
    // }

    string toString() const
    {
        string ret = s;

        reverse(ret.begin(), ret.end());

        return (sign == -1 ? "-" : "") + ret;
    }

    BigInt toBase10(int base) const
    {
        BigInt exp(1), res("0"), BASE(base);

        for (int a = 0; a < s.size(); a++) {
            int curr = (s[a] < '0' || s[a] > '9' ? (toupper(s[a]) - 'A' + 10) : (s[a] - '0'));

            res = res + (exp * BigInt(curr));
            exp = exp * BASE;
        }

        return res.normalize(sign);
    }

    BigInt toBase10(int base, BigInt mod) const
    {
        BigInt exp(1), res("0"), BASE(base);

        for (int a = 0; a < s.size(); a++) {
            int curr = (s[a] < '0' || s[a] > '9' ? (toupper(s[a]) - 'A' + 10) : (s[a] - '0'));

            res = (res + ((exp * BigInt(curr) % mod)) % mod);
            exp = ((exp * BASE) % mod);
        }

        return res.normalize(sign);
    }

    string convertToBase(int base) const
    {
        BigInt ZERO(0), BASE(base), x = *this;
        string modes = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

        if (x == ZERO)
            return "0";

        string res = "";

        while (x > ZERO) {
            BigInt mod = x % BASE;

            x = x - mod;

            if (x > ZERO)
                x = x / BASE;

            res = modes[stoi(mod.toString())] + res;
        }

        return res;
    }

    BigInt toBase(int base) const
    {
        return BigInt(this->convertToBase(base));
    }

    friend ostream& operator<<(ostream& os, const BigInt& x)
    {
        os << x.toString();

        return os;
    }

    friend istream& operator>>(istream& is, BigInt& x)
    {
        std::string str;
        is >> str;

        x = str;

        return is;
    }
};

namespace std
{
    _NODISCARD inline string to_string(BigInt const& _Val)
    {
        return _Val.toString();
    }

    template <typename T>
    T powi(T const& val, T const& exp)
    {
        T result = 1, e = exp;

        if ((e & T(1)) == T(1))
        {
            result *= val;
            --e;
        }

        for (T f = 2, g = val * val; e > 1; f = 2, g = val * val)
        {
            while (e >= f)
            {
                result *= g;
                e -= f;
                g *= g;
                f *= 2;
            }
        }

        return result;
    }

    template <typename T>
    T nthrooti(T const& val, T const& N)
    {
        if (val <= 0)
            return 0;

        T Nmin1 = N - 1;
        T n = (val / N) + 1;
        T n1 = (Nmin1 * n + (val / powi(n, Nmin1))) / N;

        while (n1 < n)
        {
            n = n1;
            n1 = (Nmin1 * n + (val / powi(n, Nmin1))) / N;
        }

        return n;
    }

    // Utilizing optimization found in replies by Simon at https://www.johndcook.com/blog/2008/11/17/fast-way-to-test-whether-a-number-is-a-square/
    template <typename T>
    bool issquare(T const& n)
    {
        if ((0x202021202030213L & (1L << (int)(n & 63))) > 0)
        {
            long t = (long)std::sqrt((double) n);
            return t * t == n;
        }

        return false;
    }

    // Utilizing optimization found at https://mersenneforum.org/showpost.php?p=110896
    template <>
    __declspec(noinline) bool issquare(BigInt const& n)
    {
        {
            BigInt m = n & 127;
            if (((m * 0x8bc40d7d) & (m * 0xa1e2f5d1) & 0x14020a) != 0) return false;
        }

        unsigned long long largeMod = std::stoull((n % std::to_string(63ULL * 25 * 11 * 17 * 19 * 23 * 31)).toString()); // SLOW, bigint modulus

        // residues mod 63. 75% rejection
        unsigned long long m = largeMod % 63; // fast, all 32-bit math
        if ((m * 0x3d491df7) & (m * 0xc824a9f9) & 0x10f14008) return false;

        // residues mod 25. 56% rejection
        m = largeMod % 25;
        if ((m * 0x1929fc1b) & (m * 0x4c9ea3b2) & 0x51001005) return false;

        // residues mod 31. 48.4% rejection
        //  Bloom filter has a little different form to keep it perfect
        m = 0xd10d829a * (largeMod % 31);
        if (m & (m + 0x672a5354) & 0x21025115) return false;

        // residues mod 23. 47.8% rejection
        m = largeMod % 23;
        if ((m * 0x7bd28629) & (m * 0xe7180889) & 0xf8300) return false;

        // residues mod 19. 47.3% rejection
        m = largeMod % 19;
        if ((m * 0x1b8bead3) & (m * 0x4d75a124) & 0x4280082b) return false;

        // residues mod 17. 47.1% rejection
        m = largeMod % 17;
        if ((m * 0x6736f323) & (m * 0x9b1d499) & 0xc0000300) return false;

        // residues mod 11. 45.5% rejection
        m = largeMod % 11;
        if ((m * 0xabf1a3a7) & (m * 0x2612bf93) & 0x45854000) return false;

        BigInt root = nthrooti(n, BigInt(2));
        return root * root == n;
    }
}

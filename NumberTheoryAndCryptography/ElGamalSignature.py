import FileIO as fio


# The Extend Euler algorithm, only returning necessary information for the multiplicative inverse
def __euler_extended(a, b):
    mat = [1, 0, 0, 1]

    fa = a
    fb = b
    gcd = a if b != 0 and a % b != 0 else b

    # Perform the first step of the euler algorithm
    q = int(a / b)
    r = a - int(q * b)

    a = b
    b = r

    if r != 0:
        # Update the extended matrix
        mat = [0, 1, 1, q]

        while r != 0:
            gcd = r

            q = int(a / b)
            r = a - int(q * b)

            a = b
            b = r

            if r != 0:
                # Calculate the updated extended matrix
                m0 = mat[0]
                m1 = mat[1]
                mat[0] = mat[2]
                mat[1] = mat[3]
                mat[2] = m0 - q * mat[2]
                mat[3] = m1 - q * mat[3]

    inv = mat[2] if fa != 0 and fb % fa != 0 else 1
    return gcd, inv


# Solves for x in ax == 1 (mod m)
def __multinv(a, m):
    # Get the gcd and inverse with extended euler
    gcd, inv = __euler_extended(a, m)

    if gcd != 1:
        raise Exception('No solution: gcd(' + str(a) + ', ' + str(m) + ') = ' + str(gcd) + ', when a result of 1 is required.')

    while inv < 0:
        inv += m

    return inv


def Sign(p, r, m, b, k):
    rk = pow(r, k, p)
    k_inv = __multinv(k, p - 1)
    s = ((m - b * rk) * k_inv) % (p - 1)

    rb = pow(r, b, p)

    print("p = " + str(p))
    print("r = " + str(r))
    print("m = " + str(m))
    print("r^b = " + str(rb))
    print("r^k = " + str(rk))
    print("s = " + str(s))

    data = "p = " + str(p) + "\nr = " + str(r) + "\nm = " + str(m) + "\nr^b = " + str(rb) + "\nr^k = " + str(rk) + "\ns = " + str(s)
    fio.WriteFile("elgamalsignature.txt", bytes(data, 'utf-8'))
    return s


def Verify(p, r, m, rb, rk, s):
    rm = pow(r, m, p)
    num = (pow(rb, rk, p) * pow(rk, s, p)) % p

    if rm == num:
        print("The signature is valid.")
    else:
        print("The signature is invalid.")


# Uncomment when using this to sign and verify signatures
"""
r = 2184169779496
m = 2020

p = 11111111111533333600500633333511111111111
b = 1762534283766219
k = 31513
Sign(p, r, m, b, k)

q = p
rb = pow(r, b, p)
rk = pow(r, k, p)
s = 8200017365264414109672350310920061990664
Verify(q, r, m, rb, rk, s)
"""

# Team 2 verification
"""
p = 11111311111233333600700633333211111311111
r = 2184169779496
m = 2020
r_b = 9661973876153929676851584758111878327741
r_k = 2331137682018616011939870198916153058580
s = 4408721909749705254146344347965704044430
Verify(p, r, m, r_b, r_k, s)
"""


extern void printInt(int val);
extern void printFloat(float val);

extern int scanInt();
extern float scanFloat();

extern void printSpaces(int num);
extern void printNewlines(int num);


extern int gcd(int a, int b);
extern int fac(int n);
extern int fib(int n);
extern bool isprime(int n);


extern void printIntVec(int[n] vec);
extern void printFloatVec(float[n] vec);
extern void printIntMat(int[m, n] mat);
extern void printFloatMat(float[m, n] mat);
extern void scanIntVec(int[n] vec);
extern void scanFloatVec(float[n] vec);
extern void scanIntMat(int[m, n] mat);
extern void scanFloatMat(float[m, n] mat);
extern bool matMul(float[m, n] a, float[o, p] b, float[q, l] c);
extern bool queens(bool[m, n] field);


int successCount = 0;
int testCount = 0;
int testGroupCount = 0;

export int main() {
    test_gcd();
    testGroupCount = 0;
    test_fac();
    testGroupCount = 0;
    test_fib();
    testGroupCount = 0;
    test_isprime();
    testGroupCount = 0;
    test_matMul();

    printInt(successCount);
    printSpaces(1);
    printInt(testCount);
    printNewlines(1);
    return 0;
}

void test(int expected, int value) {
    bool success = expected == value;
    testCount = testCount + 1;
    testGroupCount = testGroupCount + 1;
    successCount = successCount + (int)success;
    if (!success) {
        printInt(testGroupCount);
        printSpaces(4);
        printInt(expected);
        printSpaces(1);
        printInt(value);
        printNewlines(1);
    }
}

void testB(bool expected, bool value) {
    test((int)expected, (int)value);
}

void testFMat(float[m,n] input1, float[o,p] input2, float[q,l] expected) {
    float[q,l] output = -1.0; // Values does not matter because it is zeroed by matMul
    bool success = true;

    matMul(input1, input2, output);
    for (int im = 0, 3, 1) {
        for (int in = 0, 5, 1) {
            if(output[im,in] != expected[im,in])
            {
                success = false;
                printInt(im);
                printSpaces(1);
                printInt(in);
                printSpaces(3);
                printFloat(expected[im,in]);
                printSpaces(1);
                printFloat(output[im,in]);
                printNewlines(2);
            }
        }
    }

    testCount = testCount + 1;
    testGroupCount = testGroupCount + 1;
    successCount = successCount + (int)success;
}

void test_gcd() {
    test(6, gcd(12, 18));
    test(6, gcd(18, 12));
    test(38, gcd(38, 38));
    test(0, gcd(12, 0));
    test(0, gcd(0, 5));
    test(1, gcd(34, 1));
    test(1, gcd(1, 43));
}

void test_fac() {
    test(-1, fac(-1));
    test(1, fac(0));
    test(120, fac(5));
    test(1, fac(1));
}

void test_fib() {
    test(55, fib(10));
    test(0, fib(0));
    test(0, fib(-12));
    test(1, fib(1));
    test(1, fib(2));
}

void test_isprime() {
    testB(false, isprime(-12));
    testB(false, isprime(0));
    testB(false, isprime(1));
    testB(true, isprime(2));
    testB(true, isprime(13));
    testB(false, isprime(25));
    testB(true, isprime(97));
    testB(false, isprime(121));
}

void test_matMul() {
    float[3,2] input1 = [[1.0,2.0],[3.0,4.0],[5.0,6.0]];
    float[2,5] input2 = [[1.0,2.0,3.0,4.0,5.0],[6.0,7.0,8.0,9.0,10.0]];
    float[3,5] expected = [
        [13.0,16.0,19.0,22.0,25.0],
        [27.0,34.0,41.0,48.0,55.0],
        [41.0,52.0,63.0,74.0,85.0]];
    testFMat(input1, input2, expected);
}

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

const string testCatureFileName = "/home/pi/minicom.cap";
const string expectedResultsFolder = "/home/pi/c-testsuite/tests/single-exec/";

static ifstream testCatureFile;
static string line;

bool verifyOneTest(void) {
    // first line must start with "cc /.tests/passed/"
    const string prefx = "cc /.tests/passed/";
    if (line.substr(0, prefx.length()) != prefx) {
        cerr << "result header missing!" << endl;
        return false;
    }
    string testNumber = line.substr(prefx.length(), 5);
    getline(testCatureFile, line);
    getline(testCatureFile, line);
    vector<string> testOutput;
    const string ccPrefx = "CC = ";
    while (line.substr(0, ccPrefx.length()) != ccPrefx) {
        testOutput.push_back(line);
        getline(testCatureFile, line);
    }
    string condCode = line.substr(ccPrefx.length(), 5);
    if (condCode != "0") {
        cerr << "test " << testNumber << " fail: non 0 CC" << endl;
        return false;
    }
    getline(testCatureFile, line);
    // load expected results
    ifstream expectedFile(expectedResultsFolder + testNumber + ".c.expected");
    if (!expectedFile.is_open()) {
        cerr << "expected results not fould!" << endl;
        return true;
    }
    vector<string> expected;
    string expectedLine;
    while (getline(expectedFile, expectedLine))
        expected.push_back(expectedLine);
    expectedFile.close();
    int rix = 0;
    for (auto& l : expected) {
        if (l != testOutput[rix]) {
            cerr << "test " << testNumber << " fail : '" << l << "' != '" << testOutput[rix] << "'"
                 << endl;
            return true;
        }
        ++rix;
    }
    return true;
}

int main() {
    testCatureFile.open(testCatureFileName);
    if (!testCatureFile.is_open()) {
        cerr << "no capture file!" << endl;
        return -1;
    }
    getline(testCatureFile, line);
    getline(testCatureFile, line);
    while (verifyOneTest())
        ;
    testCatureFile.close();
}

//
//  main.cpp
//  native.vm
//
//  Created by Denny C. Dai on 2013-08-27.
//  Copyright (c) 2013 Denny C. Dai. All rights reserved.
//

#include <iostream>
#include <clang-c/Index.h>

using namespace std;

int main(int argc, char *argv[])
{
    CXIndex Index = clang_createIndex(0, 0);
    CXTranslationUnit TU = clang_parseTranslationUnit(Index, 0, argv, argc, 0, 0, CXTranslationUnit_None);
    
    for(unsigned I = 0, N = clang_getNumDiagnostics(TU); I!=N; ++I){
        CXDiagnostic diag = clang_getDiagnostic(TU, I);
        CXString str = clang_formatDiagnostic(diag, clang_defaultDiagnosticDisplayOptions());
        cout << "CLANG DIAGNOSTIC: " << clang_getCString(str) << endl;
    }
    
    clang_disposeTranslationUnit(TU);
    clang_disposeIndex(Index);
    
    return 0;
}
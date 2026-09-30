#include <stdio.h>
#include <stdlib.h>

#include "resolver.h"
#include "util.h"
#include "validator.h"

int main(int argc, char *argv[]){
    if (argc != 2){
        fprintf(stderr, "Usage: %s <domain-name | IPv4-address>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *parameter = argv[1];
    StringList result;
    initStringList(&result);

    int found = 0;
    if (isValidIPv4(parameter)){
        found = reverseLookup(parameter, &result);
    } else if (isValidDomain(parameter)){
        found = forwardLookup(parameter, &result);
    }

    if (found < 0){
        fprintf(stderr, "Error: lookup failed.\n");
        freeStringList(&result);
        return EXIT_FAILURE;
    }

    printResult(&result);
    freeStringList(&result);
    return EXIT_SUCCESS;
}

#include "handler.h"
#include "protocol.h"
#include "resolver.h"
#include "util.h"
#include "validator.h"

void handleQuery(const char *request, char *response, size_t size){
    StringList result;
    initStringList(&result);

    int found = 0;
    if (isValidIPv4(request)){
        found = reverseLookup(request, &result);
    } else if (isValidDomain(request)){
        found = forwardLookup(request, &result);
    }

    if (found < 0){
        formatErrorResponse("Server error", response, size);
    } else if (result.count == 0){
        formatErrorResponse("Not found information", response, size);
    } else {
        formatSuccessResponse(&result, response, size);
    }

    freeStringList(&result);
}

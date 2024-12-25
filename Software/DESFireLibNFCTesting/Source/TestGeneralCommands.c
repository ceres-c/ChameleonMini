/* TestGeneralCommands.c */

#include <stdlib.h>
#include <stdio.h>

#include <nfc/nfc.h>

#include "LibNFCUtils.h"
#include "LibNFCWrapper.h"
#include "DesfireUtils.h"


int main(int argc, char **argv) {

    nfc_context *nfcCtxt;
    nfc_device  *nfcPnd = GetNFCDeviceDriver(&nfcCtxt);
    if (nfcPnd == NULL) {
        return EXIT_FAILURE;
    }

    if (Authenticate(nfcPnd, DESFIRE_CRYPTO_AUTHTYPE_LEGACY, MASTER_KEY_INDEX, ZERO_KEY)) {
        fprintf(stdout, "    -- !! Error authenticating !!\n");
        return EXIT_FAILURE;
    } else if (GetVersionCommand(nfcPnd)) {
        fprintf(stdout, "    -- !! GetVersion failed !!\n");
        return EXIT_FAILURE;
    }

    FreeNFCDeviceDriver(&nfcCtxt, &nfcPnd);
    return EXIT_SUCCESS;

}

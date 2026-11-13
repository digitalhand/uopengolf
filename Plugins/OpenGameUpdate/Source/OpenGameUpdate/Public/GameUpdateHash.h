// SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"

namespace GameUpdate
{
    /** SHA-256 (FIPS 180-4). Feed data with Update, then read the digest once with FinalHex. */
    class OPENGAMEUPDATE_API FSha256
    {
    public:
        FSha256();
        void Update(const uint8* Data, uint64 Size);
        /** Lowercase hex digest. */
        FString FinalHex();

    private:
        void Block(const uint8* Data);
        uint32 State[8];
        uint8 Buffer[64];
        uint32 Buffered = 0;
        uint64 TotalBytes = 0;
    };

    OPENGAMEUPDATE_API FString Sha256Hex(TConstArrayView<uint8> Data);
    /** Reads the file in chunks, so it is fine for large installers; use off the game thread. Empty if unreadable. */
    OPENGAMEUPDATE_API FString Sha256OfFile(const FString& Path);
}

// SPDX-License-Identifier: MIT
// SHA-256 from FIPS 180-4. The engine has no portable SHA-256, and this keeps the plugin free of OpenSSL.
#include "GameUpdateHash.h"
#include "HAL/FileManager.h"
#include "Serialization/Archive.h"

namespace GameUpdate
{
    namespace
    {
        constexpr uint32 K[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

        constexpr uint32 Rotr(uint32 X, uint32 N) { return (X >> N) | (X << (32 - N)); }
    }

    FSha256::FSha256()
        : State{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19}
    {
    }

    void FSha256::Block(const uint8* Data)
    {
        uint32 W[64];
        for (int32 I = 0; I < 16; ++I)
            W[I] = uint32(Data[I * 4]) << 24 | uint32(Data[I * 4 + 1]) << 16 | uint32(Data[I * 4 + 2]) << 8 | uint32(Data[I * 4 + 3]);
        for (int32 I = 16; I < 64; ++I)
        {
            const uint32 S0 = Rotr(W[I - 15], 7) ^ Rotr(W[I - 15], 18) ^ (W[I - 15] >> 3);
            const uint32 S1 = Rotr(W[I - 2], 17) ^ Rotr(W[I - 2], 19) ^ (W[I - 2] >> 10);
            W[I] = W[I - 16] + S0 + W[I - 7] + S1;
        }
        uint32 A = State[0], B = State[1], C = State[2], D = State[3], E = State[4], F = State[5], G = State[6], H = State[7];
        for (int32 I = 0; I < 64; ++I)
        {
            const uint32 T1 = H + (Rotr(E, 6) ^ Rotr(E, 11) ^ Rotr(E, 25)) + ((E & F) ^ (~E & G)) + K[I] + W[I];
            const uint32 T2 = (Rotr(A, 2) ^ Rotr(A, 13) ^ Rotr(A, 22)) + ((A & B) ^ (A & C) ^ (B & C));
            H = G; G = F; F = E; E = D + T1; D = C; C = B; B = A; A = T1 + T2;
        }
        State[0] += A; State[1] += B; State[2] += C; State[3] += D; State[4] += E; State[5] += F; State[6] += G; State[7] += H;
    }

    void FSha256::Update(const uint8* Data, uint64 Size)
    {
        TotalBytes += Size;
        while (Size > 0)
        {
            const uint32 Take = uint32(FMath::Min<uint64>(64 - Buffered, Size));
            FMemory::Memcpy(Buffer + Buffered, Data, Take);
            Buffered += Take; Data += Take; Size -= Take;
            if (Buffered == 64) { Block(Buffer); Buffered = 0; }
        }
    }

    FString FSha256::FinalHex()
    {
        const uint64 Bits = TotalBytes * 8;
        const uint8 One = 0x80, Zero = 0;
        Update(&One, 1);
        while (Buffered != 56) Update(&Zero, 1);
        uint8 Length[8];
        for (int32 I = 0; I < 8; ++I) Length[I] = uint8(Bits >> (56 - 8 * I));
        Update(Length, 8);
        FString Hex;
        for (uint32 Word : State) Hex += FString::Printf(TEXT("%08x"), Word);
        return Hex;
    }

    FString Sha256Hex(TConstArrayView<uint8> Data)
    {
        FSha256 Hash;
        Hash.Update(Data.GetData(), Data.Num());
        return Hash.FinalHex();
    }

    FString Sha256OfFile(const FString& Path)
    {
        TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*Path));
        if (!Reader) return {};
        FSha256 Hash;
        TArray<uint8> Chunk;
        Chunk.SetNumUninitialized(1 << 20);
        for (int64 Left = Reader->TotalSize(); Left > 0;)
        {
            const int64 Take = FMath::Min<int64>(Left, Chunk.Num());
            Reader->Serialize(Chunk.GetData(), Take);
            if (Reader->IsError()) return {};
            Hash.Update(Chunk.GetData(), Take);
            Left -= Take;
        }
        return Reader->Close() ? Hash.FinalHex() : FString();
    }
}

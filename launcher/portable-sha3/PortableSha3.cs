// SPDX-License-Identifier: GPL-3.0-or-later
// FIPS 202 SHA3-256 for hosts where .NET's operating-system provider lacks SHA3.
// Algorithm specification: https://keccak.team/keccak_specs_summary.html
using System;
using System.Buffers.Binary;
using System.IO;
using System.Numerics;

namespace Fc27.Packaging;

public static class PortableSha3
{
    public static bool IsSupported => true;

    public static byte[] HashData(ReadOnlySpan<byte> data)
    {
        if (System.Security.Cryptography.SHA3_256.IsSupported)
            return System.Security.Cryptography.SHA3_256.HashData(data);
        using var hash = new PortableSha3Incremental();
        hash.AppendData(data);
        return hash.GetHashAndReset();
    }

    public static int HashData(ReadOnlySpan<byte> data, Span<byte> destination)
    {
        if (destination.Length < 32)
            throw new ArgumentException("SHA3-256 requires 32 output bytes.", nameof(destination));
        HashData(data).CopyTo(destination);
        return 32;
    }

    public static byte[] HashData(Stream stream)
    {
        using var hash = new PortableSha3Incremental();
        byte[] buffer = new byte[65536];
        int count;
        while ((count = stream.Read(buffer)) != 0)
            hash.AppendData(buffer.AsSpan(0, count));
        return hash.GetHashAndReset();
    }
}

public sealed class PortableSha3Incremental : IDisposable
{
    private const int RateBytes = 136;
    private readonly ulong[] state = new ulong[25];
    private readonly byte[] pending = new byte[RateBytes];
    private int pendingLength;
    private bool disposed;

    private static ReadOnlySpan<int> Rotations => [
        0, 1, 62, 28, 27, 36, 44, 6, 55, 20, 3, 10, 43, 25, 39,
        41, 45, 15, 21, 8, 18, 2, 61, 56, 14];
    private static ReadOnlySpan<ulong> RoundConstants => [
        0x0000000000000001UL, 0x0000000000008082UL, 0x800000000000808aUL,
        0x8000000080008000UL, 0x000000000000808bUL, 0x0000000080000001UL,
        0x8000000080008081UL, 0x8000000000008009UL, 0x000000000000008aUL,
        0x0000000000000088UL, 0x0000000080008009UL, 0x000000008000000aUL,
        0x000000008000808bUL, 0x800000000000008bUL, 0x8000000000008089UL,
        0x8000000000008003UL, 0x8000000000008002UL, 0x8000000000000080UL,
        0x000000000000800aUL, 0x800000008000000aUL, 0x8000000080008081UL,
        0x8000000000008080UL, 0x0000000080000001UL, 0x8000000080008008UL];

    public void AppendData(ReadOnlySpan<byte> input)
    {
        ObjectDisposedException.ThrowIf(disposed, this);
        while (!input.IsEmpty)
        {
            if (pendingLength == 0 && input.Length >= RateBytes)
            {
                Absorb(input[..RateBytes]);
                input = input[RateBytes..];
                continue;
            }
            int count = Math.Min(RateBytes - pendingLength, input.Length);
            input[..count].CopyTo(pending.AsSpan(pendingLength));
            pendingLength += count;
            input = input[count..];
            if (pendingLength == RateBytes)
            {
                Absorb(pending);
                pendingLength = 0;
                pending.AsSpan().Clear();
            }
        }
    }

    public byte[] GetHashAndReset()
    {
        ObjectDisposedException.ThrowIf(disposed, this);
        pending.AsSpan(pendingLength).Clear();
        pending[pendingLength] = 0x06;
        pending[RateBytes - 1] |= 0x80;
        Absorb(pending);
        byte[] result = new byte[32];
        for (int i = 0; i < 4; ++i)
            BinaryPrimitives.WriteUInt64LittleEndian(result.AsSpan(i * 8), state[i]);
        Reset();
        return result;
    }

    private void Absorb(ReadOnlySpan<byte> block)
    {
        for (int i = 0; i < RateBytes / 8; ++i)
            state[i] ^= BinaryPrimitives.ReadUInt64LittleEndian(block[(i * 8)..]);
        Span<ulong> c = stackalloc ulong[5];
        Span<ulong> d = stackalloc ulong[5];
        Span<ulong> b = stackalloc ulong[25];
        foreach (ulong rc in RoundConstants)
        {
            for (int x = 0; x < 5; ++x)
                c[x] = state[x] ^ state[x+5] ^ state[x+10] ^ state[x+15] ^ state[x+20];
            for (int x = 0; x < 5; ++x)
                d[x] = c[(x+4)%5] ^ BitOperations.RotateLeft(c[(x+1)%5], 1);
            for (int y = 0; y < 5; ++y)
                for (int x = 0; x < 5; ++x)
                {
                    int i = x + 5*y;
                    state[i] ^= d[x];
                    b[y + 5*((2*x+3*y)%5)] = BitOperations.RotateLeft(state[i], Rotations[i]);
                }
            for (int y = 0; y < 5; ++y)
                for (int x = 0; x < 5; ++x)
                    state[x+5*y] = b[x+5*y] ^ (~b[(x+1)%5+5*y] & b[(x+2)%5+5*y]);
            state[0] ^= rc;
        }
    }

    private void Reset()
    {
        state.AsSpan().Clear();
        pending.AsSpan().Clear();
        pendingLength = 0;
    }

    public void Dispose()
    {
        Reset();
        disposed = true;
    }
}

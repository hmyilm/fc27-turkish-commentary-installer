// SPDX-License-Identifier: GPL-3.0-or-later
using Fc27.Packaging;
using System.Text.Json;

using var vectors = JsonDocument.Parse(File.ReadAllText(args[0]));
int count = 0;
foreach (var vector in vectors.RootElement.EnumerateArray())
{
    byte[] input = Convert.FromBase64String(vector.GetProperty("data").GetString()!);
    string expected = vector.GetProperty("sha3_256").GetString()!;
    void Check(byte[] actual)
    {
        if (Convert.ToHexStringLower(actual) != expected)
            throw new Exception($"SHA3 mismatch for vector {count}, length {input.Length}");
    }
    Check(PortableSha3.HashData(input));
    using var stream = new MemoryStream(input);
    Check(PortableSha3.HashData(stream));
    byte[] destination = new byte[40];
    if (PortableSha3.HashData(input, destination) != 32)
        throw new Exception("Wrong digest length");
    Check(destination[..32]);
    foreach (int chunk in new[] {1, 7, 135, 136, 137, 4096, 65536})
    {
        using var hash = new PortableSha3Incremental();
        for (int round = 0; round < 2; ++round)
        {
            for (int offset = 0; offset < input.Length; offset += chunk)
                hash.AppendData(input.AsSpan(offset, Math.Min(chunk, input.Length-offset)));
            Check(hash.GetHashAndReset());
        }
    }
    ++count;
}
Console.WriteLine($"PASS: {count} Python hashlib vectors; byte/span/stream/incremental/reset; OS SHA3 supported: {System.Security.Cryptography.SHA3_256.IsSupported}");

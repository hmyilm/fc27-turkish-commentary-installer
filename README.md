# FC27 PS5 Türkçe Spiker Kurucusu

PPSA34015 için yerel PS5 ELF kurucusu. Hazır Türkçe spiker ZIP'ini USB'den veya PS5 depolamasından okur; 10 Türkçe veri dosyasını doğrulayarak oyun klasörüne kurar ve oyunun kendi `ampr_emu.index` dosyasındaki ilgili boyut kayıtlarını düzeltir.

**GitHub paketinde oyun, spiker sesleri, unlock veya backport dosyaları bulunmaz.** Kurucu, gerekli ses dosyalarını ayrıca edinmiş kullanıcılar içindir. ZIP'i internetten indirmez, FTP'ye bağlanmaz ve kurulum için PC'de Python çalıştırmayı gerektirmez.

## Uyumluluk ve test durumu

| Hedef | Durum |
| --- | --- |
| PPSA34015 / v01.000.004 | Kaynak kurulumda çevrimdışı maç ve Türkçe spiker çalıştı. ZIP'ten yeniden kurulum sonrası maç da kullanıcı tarafından doğrulandı. |
| PPSA34015 / v01.000.003 | Kurucu kabul eder; bu sürümde oyun içi test yapılmadı. |
| Native ELF kurucusu | 7 Ekim derlemesi PS5 dosya/indeks testlerini geçti. 8 Ekim yayın derlemesinde yalnız derleme yolu metinleri sadeleştirildi; yeni ikili için konsol testi bekliyor. Ayrıntılar [test notlarında](docs/TEST_DURUMU.md). |
| PKG başlatıcısı | Hazırlanıyor; ELF test sonuçları PKG başlatıcısının doğrulandığı anlamına gelmez. |
| Diğer oyun kimlikleri / sürümler | Kabul edilmez. |

Hedef oyun yazılabilir **klasör biçiminde** olmalı ve AMPRIDX3 indeksinde gereken 10 Türkçe dosya yolu bulunmalı. Sıkıştırılmış `.ffpfsc` veya exFAT oyun görüntüsünü doğrudan düzenlemez. Farklı backport ve yükleyicilerde çalışacağına dair genel bir garanti yoktur.

## USB ile hızlı kurulum

1. FC27'yi tamamen kapatın.
2. USB'ye şu iki dosyayı koyun:

   ```text
   USB/
   ├── pldmgr/
   │   └── FC27_TR_KUR.elf
   └── FC27_TR/
       └── FC27_TURKCE_SPIKER_PPSA34015_v01.000.004.zip
   ```

3. USB'yi PS5'e takın. Payload Manager'da `FC27_TR_KUR` ELF'ini başlatın. Listelenmiyorsa listeyi yenileyin.
4. Kurulum tamamlandı bildirimi gelene kadar bekleyin. Oyunu açıp spiker dilini Türkçe seçin ve çevrimdışı bir maç deneyin.

Hedef klasör seçimi, kontrol modu ve hata çözümü için [ayrıntılı kurulum rehberini](docs/KURULUM.md) okuyun.

## İndirme ve dosya doğrulama

Kaynak kod ve yayınlanan araçlar: [GitHub sürümleri](https://github.com/hmyilm/fc27-turkish-commentary-installer/releases).

8 Ekim yayın derlemesi `FC27_TR_KUR.elf`: **191.808 bayt**, SHA256:

```text
68afa145186704f44065684a7ab7722f0f1e3269db0a1064fd00afabcb73d048
```

Bu derleme kişisel derleme yollarını içermez. Yedi nesne dosyasının çalıştırılabilir bölümleri önceki PS5 testinden geçen derlemeyle birebir aynı; yeni ikilinin konsolda yeniden testi henüz yapılmadı. [Derleme karşılaştırması](tests/build-normalization-results.txt).

Türkçe veri ZIP'i bu depoda ve GitHub sürümlerinde bulunmaz.

## Kurucu ne yapar?

- Oyun kimliğini, sürümünü, indeks yapısını ve oyunun kapalı olduğunu kontrol eder.
- ZIP'teki bilinen 10 dosyayı akış hâlinde okur; boyut, ZIP CRC32 ve SHA256 değerlerini doğrular.
- Değişecek dosyaları önce geçici dosyalara çıkarır. Asıl dosyalara geçmeden doğrulama tamamlanır.
- İndekste yalnızca bu Türkçe dosyaların sıfır kalan boyut alanlarını günceller. Diğer kayıtları ve hash tablosunu korur.
- Kurulum hatasında geçici yedeklerle önceki dosyaları geri getirir. Başarılı kurulumdan sonra geçici dosyaları temizler.
- Zaten doğru olan dosyaları tekrar kopyalamaz. `/data/FC27_TR` varsa ve yazılabiliyorsa sonucu `installer.log` dosyasına kaydeder. Kontrol modu oyun dosyalarını değiştirmez; günlük yazabilir.

İşlem sırasında konsolu kapatmayın veya USB'yi çıkarmayın. Elektrik kesintisi sonrasında kalan geçici dosyalar varsa kurucu işlemi durdurur; ayrıntılar rehberdedir.

## PC ile alternatif

`tools/TR_INDEX_DUZELT.py`, PS5'ten alınmış yerel indeks kopyasını yeni bir dosyaya dönüştürür. FTP bağlantısı veya otomatik kurulum yapmaz. Kullanım [kurulum rehberinde](docs/KURULUM.md#pc-ile-manuel-alternatif).

## Kaynak kod ve lisans

Proje kodu **GPL-3.0-or-later** lisanslıdır; tam metin [LICENSE](LICENSE) dosyasındadır. Dahil edilen bileşenlerin lisansları [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) dosyasında belirtilir. Bu lisans oyun ve ses varlıklarına hak vermez.

### ELF'i derlemek

[ps5-payload-dev SDK v0.43](https://github.com/ps5-payload-dev/sdk/releases/tag/v0.43) ve LLVM/Clang gerekir. Depo klasöründe, SDK dizininizi belirterek çalıştırın:

```sh
PS5_PAYLOAD_SDK="/SDK/ps5-payload-sdk" sh ./build.sh
```

SDK'nın LLVM kurulumunu bulamadığı ortamlarda `LLVM_CONFIG` değerini kendi `llvm-config` dosyanıza ayarlayın:

```sh
LLVM_CONFIG="/LLVM/bin/llvm-config" PS5_PAYLOAD_SDK="/SDK/ps5-payload-sdk" sh ./build.sh
```

Çıktı `build/FC27_TR_KUR.elf` olur. Test edilen derleme LLVM 23.1.2 ile yapıldı.

Derleme betiği kaynak klasörünü `-ffile-prefix-map` ile göreli gösterir; geliştiricinin kişisel klasör yolu ELF içine yazılmaz.

Bilgisayarda test derlemesi:

```sh
sh ./build.sh --host
./build/FC27_TR_KUR_host --check --game /oyun/klasoru --zip /paket/FC27_TURKCE_SPIKER_PPSA34015_v01.000.004.zip
```

`/SDK`, `/LLVM`, `/oyun` ve `/paket` yollarını kendi ortamınıza göre değiştirin. Host derlemesi PS5 işlem/API kontrolünü içermez; PS5 için kullanılamaz.

# FC27 PS5 Türkçe Spiker Kurucusu

PPSA34015 için yerel PS5 ELF kurucusu. Hazır Türkçe spiker ZIP'ini USB'den veya PS5 depolamasından okur; 10 Türkçe veri dosyasını doğrulayarak oyun klasörüne kurar ve oyunun kendi `ampr_emu.index` dosyasındaki ilgili boyut kayıtlarını düzeltir.

**v0.1.1-beta — USB + DATA güncellemesi:** Harici SSD’lere ek olarak PS5’in `/data` dizini de alt klasörleriyle aranır. `etaHEN` klasörü veya belirli bir oyun klasörü adı zorunlu değil; oyun, içindeki kimlik ve veri dosyalarıyla tanınır. Bu güncellemenin 81 klasör arama testi ve tam kurulum/geri alma entegrasyon testi bilgisayarda geçti. PS5 testi henüz yapılmadı.

**Daha önce v0.1.1-beta indirdiyseniz güncel `FC27_TR_KURUCU_v0.1.1-beta_USB_DATA.zip` paketini indirin.** İçindeki `FC27_TR_KUR.elf` hem USB hem `/data` aramasını içerir. Sürüm numarası aynı kaldı; ilk v0.1.1-beta dosyası yalnız belirli dahili oyun dizinlerini arıyordu. Yeni ELF’in başlangıç bildiriminde **`USB+DATA`** yazar. Ses ZIP’ini yeniden indirmeniz gerekmez.

**GitHub paketinde oyun, spiker sesleri, unlock veya backport dosyaları bulunmaz.** Kurucu, gerekli ses dosyalarını ayrıca edinmiş kullanıcılar içindir. ZIP'i internetten indirmez, FTP'ye bağlanmaz ve kurulum için PC'de Python çalıştırmayı gerektirmez.

## Uyumluluk ve test durumu

| Hedef | Durum |
| --- | --- |
| PPSA34015 / v01.000.004 | Kaynak kurulumda çevrimdışı maç ve Türkçe spiker çalıştı. ZIP'ten yeniden kurulum sonrası maç da kullanıcı tarafından doğrulandı. |
| PPSA34015 / v01.000.003 | Kurucu kabul eder; bu sürümde oyun içi test yapılmadı. |
| Native ELF kurucusu | Önceki 7 Ekim derlemesi PS5 dosya/indeks testlerini geçti. v0.1.1-beta USB + DATA güncellemesinin 81 klasör arama testi ve tam kurulum/geri alma entegrasyon testi bilgisayarda geçti. PS5 testi henüz yapılmadı. Ayrıntılar [test notlarında](docs/TEST_DURUMU.md). |
| PKG başlatıcısı | Hazırlanıyor; ELF test sonuçları PKG başlatıcısının doğrulandığı anlamına gelmez. |
| Diğer oyun kimlikleri / sürümler | Kabul edilmez. |

Hedef oyun yazılabilir **klasör biçiminde** olmalı ve AMPRIDX3 indeksinde gereken 10 Türkçe dosya yolu bulunmalı. **exFAT biçimli harici SSD’deki açılmış oyun klasörü desteklenir; oyun PS5’in iç depolamasında olmak zorunda değildir.** `.ffpfsc` veya exFAT oyun görüntüsünü doğrudan düzenlemez. Farklı backport ve yükleyicilerde çalışacağına dair genel bir garanti yoktur.

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

Oyun, USB0–USB7 disklerinin kökünden en fazla dört klasör; PS5’in `/data` kökünden en fazla 32 klasör derinliğinde otomatik aranır. Örneğin SSD kökündeki `FC27`, `games/FC27`, `OnionHEN/games/FC27` veya PS5’te `/data/oyunlarim/FC27` kullanılabilir. Klasörün adından bağımsız olarak içindeki `sce_sys/param.json`, `ampr_emu.index` ve `Data/Ps5` kontrol edilir. Birden fazla uygun oyun varsa veya tarama sınırına ulaşılırsa `install.conf` ile hedef seçin.

Dahili arama konumları, hedef klasör seçimi, kontrol modu ve hata çözümü için [ayrıntılı kurulum rehberini](docs/KURULUM.md) okuyun.

## İndirme ve dosya doğrulama

Önerilen güncel paket: [FC27_TR_KURUCU_v0.1.1-beta_USB_DATA.zip](https://github.com/hmyilm/fc27-turkish-commentary-installer/releases/download/v0.1.1-beta/FC27_TR_KURUCU_v0.1.1-beta_USB_DATA.zip). İçinden çıkan `FC27_TR_KUR.elf` dosyasını kullanın.

- [Güncel kaynak paketi: FC27_TR_KAYNAK_v0.1.1-beta_USB_DATA.zip](https://github.com/hmyilm/fc27-turkish-commentary-installer/releases/download/v0.1.1-beta/FC27_TR_KAYNAK_v0.1.1-beta_USB_DATA.zip)
- [Aynı v0.1.1-beta yayınındaki tüm dosyalar](https://github.com/hmyilm/fc27-turkish-commentary-installer/releases/tag/v0.1.1-beta)

Önceki dosyalar aynı yayında duruyor; **`USB_DATA` içeren güncel paketi seçin.** Tek başına `FC27_TR_KUR_USB_DATA.elf` indirirseniz USB’ye koyarken adını `FC27_TR_KUR.elf` yapın. GitHub’ın otomatik **Source code (zip/tar.gz)** dosyaları ilk v0.1.1-beta etiketindeki kodu içerir; güncel kaynak için yukarıdaki özel kaynak ZIP’ini veya deponun `main` dalını kullanın.

v0.1.1-beta USB + DATA güncellemesi `FC27_TR_KUR.elf`: **192.176 bayt**, SHA256:

```text
a553f01fc79bf8b9b14ef5eb7179c6c9d4fd60aa0fc6427cb6e3ba97e5bbfae7
```

Bu ELF, ilk yayımlanan v0.1.1-beta ELF’iyle aynı dosya değildir; `/data` genelinde arama eklenmiştir. Bu güncellemenin PS5 testi henüz yapılmadı. Önceki derlemelerin hash ve test kayıtları [test durumunda](docs/TEST_DURUMU.md) korunur.

Türkçe veri ZIP'i bu depoda ve GitHub sürümlerinde bulunmaz. [MediaFire'dan indirin — yaklaşık 1,37 GB](https://www.mediafire.com/file/y81auk6wpsdlbw8/FC27_TURKCE_SPIKER_PPSA34015_v01.000.004.zip/file). ZIP'i açmadan ve adını değiştirmeden kullanın; içindeki eski metinler PC ile manuel alternatifi anlatır, ELF için [güncel rehberi](docs/KURULUM.md) izleyin.

Ses ZIP'i SHA256:

```text
78d3a21b8accf760dcef0328be29e0d715df2af8080f6b4bbf0bf78e054bf3f4
```

[MCPSP paylaşım konusu](https://www.mcpsp.com/threads/fc-27-ps5-turkce-spiker-dosyalari-ve-elf-kurucusu-ppsa34015-v004.95660/) 8 Ekim 2026'da yayımlandı.

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

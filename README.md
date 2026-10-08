# FC27 PS5 Türkçe Spiker Kurucusu

**v0.2.0-beta — klasör + oyun görüntüsü desteği.** PPSA34015 için PS5 üzerinde çalışan ELF kurucusu. Yerel Türkçe ses ZIP’inden 10 dosyayı doğrulayarak kurar; oyunun kendi AMPR indeksindeki ilgili boyut kayıtlarını düzeltir.

## Bu sürümde ne değişti?

- Açılmış oyun klasörleri USB/SSD’de ve PS5’in `/data` dizini altında bulunur; `etaHEN` adı zorunlu değildir.
- **ShadowMount+ ile kullanılan oyun görüntülerine ayrı dosya katmanı üzerinden kurulum eklendi.** Oyunu Game Compressor ile açmak veya yeniden sıkıştırmak gerekmez. Görüntünün içine yazılmaz.
- Mevcut backport klasörü varsa gerçek seçili klasör kullanılır, diğer dosyalar korunur. Yoksa `/data/homebrew/backports/PPSA34015` oluşturulur.
- Yeni klasörün ShadowMount tarafından kabul edilmesi için gereken bekleme otomatik yapılır. İndeks denetimi, iptal ve hata sonrası geri alma geliştirildi.

**8 Ekim konsol testi:** FW 13.20 + ShadowMount+ 1.7beta3 + PPSA34015 v004, `.ffpfsc` içinde exFAT görüntüsü. ELF 10 dosyayı kurdu, ZIP CRC/SHA256 ve kurulu dosyaların tam geri okuması geçti. Oyunun gördüğü konumdaki dosyalar da doğrulandı. Yeniden bağlama testi sonrası kullanıcı **Türkçe spikerle maça girdiğini doğruladı**. Ayrıntılar [test durumunda](docs/TEST_DURUMU.md).

## İndir

- [Kurucu ZIP — v0.2.0-beta](https://github.com/hmyilm/fc27-turkish-commentary-installer/releases/download/v0.2.0-beta/FC27_TR_KURUCU_v0.2.0-beta.zip)
- [Kaynak kod ZIP — v0.2.0-beta](https://github.com/hmyilm/fc27-turkish-commentary-installer/releases/download/v0.2.0-beta/FC27_TR_KAYNAK_v0.2.0-beta.zip)
- [Sürümler](https://github.com/hmyilm/fc27-turkish-commentary-installer/releases)
- [Türkçe ses ZIP’i — yaklaşık 1,37 GB](https://www.mediafire.com/file/y81auk6wpsdlbw8/FC27_TURKCE_SPIKER_PPSA34015_v01.000.004.zip/file)

**Ses ZIP’i değişmedi; tekrar indirmeniz gerekmez.** Küçük kurucu ZIP’ini açın. 1,37 GB ses ZIP’ini açmadan ve adını değiştirmeden kullanın.

GitHub paketinde oyun, ses varlıkları, unlock veya backport kütüphaneleri yoktur. Kurucu ZIP’i internetten indirmez; PC’de Python veya FTP bağlantısı gerektirmez. Görüntü modunda PS5’in kendi yerel ShadowMount servisiyle konuşur.

## Hızlı kurulum

1. FC27’yi tamamen kapatın. Görüntü kullanıyorsanız ShadowMount+ çalışıyor ve oyun kütüphanede görünüyor olmalı.
2. USB’ye şu dosyaları koyun:

   ```text
   /pldmgr/FC27_TR_KUR.elf
   /FC27_TR/FC27_TURKCE_SPIKER_PPSA34015_v01.000.004.zip
   ```

3. Payload Manager’dan ELF’i başlatın. Başlangıçta **`v0.2.0-beta (KLASOR+GORUNTU)`** görünmeli.
4. Kurulum tamamlanınca oyunda Türkçe spikeri seçip çevrimdışı maç deneyin.

Kurulum için yaklaşık 2 GB boş alan gerekir; ses ZIP’i aynı diskteyse onun için ayrıca alan gerekir. İşlem sırasında oyunu açmayın, başka araçla bağlama/sıkıştırma yapmayın veya USB’yi çıkarmayın.

## Uyumluluk

| Durum | Sonuç |
| --- | --- |
| PPSA34015 v004, kaynak klasör kurulumu | Önceki kurulumda Türkçe maç doğrulandı. |
| PPSA34015 v004, FFPFSC içindeki exFAT, FW 13.20 / ShadowMount 1.7beta3 | Bu ELF ile kurulum ve Türkçe maç doğrulandı. |
| Tek başına `.exfat` görüntüsü | Aynı yöntem için kod desteği var; ayrı konsol testi yapılmadı. |
| v01.000.003 | Kimlik/indeks uygunsa kabul edilir; oyun içi test yapılmadı. |
| FW 13.60 | Bu kurucu o firmware üzerinde denenmedi. Game Compressor gerektirmez. |
| Başka indirme/backport, özellikle eski Pippo paketi | Genel çalışma veya maça giriş garantisi verilmez. |
| `ampr_assets.index` içeren AMPR varlık paketleri | Görüntü modu güvenli biçimde durur; henüz desteklenmez. |
| Başka oyun kimliği veya sürüm | Kabul edilmez. |

Kurucu Türkçe içerik ekler. Eksik genel oyun dosyalarını, bozuk başlangıç dosyasını veya mevcut maç yükleme sorununu giderdiğini iddia etmez. Tam maç, konsolu yeniden başlatma ve farklı paketlerde uzun kullanım henüz doğrulanmadı.

## Oyun konumu

Klasör araması USB0–USB7 köklerinden dört, `/data` altında 32 klasör derinliğine kadar yapılır. Birden fazla kopya veya eksik tarama durumunda seçim istenir. Klasör bulunamazsa ShadowMount’un kayıtlı PPSA34015 görüntüsü sorgulanır.

**Klasör yedeği ve görüntü birlikte duruyorsa oynadığınız kaynağı açıkça seçin.** `/data/FC27_TR/install.conf` içindeki `game=` satırı klasörün veya ShadowMount’a kayıtlı fiziksel görüntü dosyasının tam yolu olabilir. Örneğin:

```ini
game=/data/etaHEN/games/PPSA34015.ffpfsc
```

Bağlı `/app0` veya `/mnt/shadowmnt/...` yolunu yazmayın. Ayrıntılar, kontrol modu ve hata çözümleri [kurulum rehberinde](docs/KURULUM.md).

## Doğrulama

ELF: **227.256 bayt**, SHA256:

```text
63cff8e096e70ae903f888616d603fc872d5cbb18854119c30bfdb3f92de4208
```

Ses ZIP’i SHA256:

```text
78d3a21b8accf760dcef0328be29e0d715df2af8080f6b4bbf0bf78e054bf3f4
```

Bilgisayarda 81 arama senaryosu, 55 kurulum/geri alma senaryosu, ShadowMount modülünde 442 doğrulama ve gerçek 10 dosyalık ZIP ile entegrasyon testi geçti. Bu testler farklı firmware’lerde oyun içi uyumluluk garantisi değildir.

## Kaynak kod

Proje GPL-3.0-or-later: [LICENSE](LICENSE), [üçüncü taraf bildirimleri](THIRD_PARTY_NOTICES.md). Lisans oyun/ses varlıklarına hak vermez. [Teknik görüntü desteği notları](docs/IMAGE_SUPPORT_NOTES.md).

[PS5 payload SDK v0.43](https://github.com/ps5-payload-dev/sdk/releases/tag/v0.43) ve LLVM ile:

```sh
PS5_PAYLOAD_SDK="/SDK/ps5-payload-sdk" sh ./build.sh
```

Gerekirse `LLVM_CONFIG` kendi `llvm-config` yolunuza ayarlanır. Çıktı `build/FC27_TR_KUR.elf`. Bilgisayar derlemesi `sh ./build.sh --host`; PS5’te kullanılamaz.

[MCPSP paylaşım konusu](https://www.mcpsp.com/threads/fc-27-ps5-turkce-spiker-dosyalari-ve-elf-kurucusu-ppsa34015-v004.95660/)

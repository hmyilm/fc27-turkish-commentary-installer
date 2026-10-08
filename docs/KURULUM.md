# Kurulum rehberi — v0.2.0-beta

PPSA34015, v01.000.003 / v01.000.004 kabul edilir. Ses kaynağı v004’tür; v003’te oyun içi test yapılmadı. [Doğrulanan koşullar](TEST_DURUMU.md).

## Gerekenler

- PS5’te ELF çalıştırabilen ortam ve Payload Manager.
- Açılmış, yazılabilir oyun klasörü **veya** ShadowMount+ tarafından kayıtlı oyun görüntüsü.
- Görüntü modu için ShadowMount+ 1.7beta3’ün kullandığı yerel API açık olmalı: varsayılan `127.0.0.1:10101`. İnternete veya yerel ağa açılması gerekmez.
- `FC27_TURKCE_SPIKER_PPSA34015_v01.000.004.zip`: yaklaşık 1,37 GB. ZIP açılmadan kullanılır.
- Kurulum hedefinde yaklaşık 2 GB boş alan. ZIP aynı depolamada tutulacaksa ayrıca alan gerekir. Açılmış 10 ses dosyası toplam 1.503.216.738 bayttır.

## Kısa kurulum

1. FC27 üzerinde Options → **Oyunu Kapat**. Ana ekrana dönmek tek başına yeterli değildir.
2. Küçük **kurucu ZIP’ini** açın. İçinden çıkan ELF’i ve açılmamış **ses ZIP’ini** USB’ye yerleştirin:

   ```text
   /pldmgr/FC27_TR_KUR.elf
   /FC27_TR/FC27_TURKCE_SPIKER_PPSA34015_v01.000.004.zip
   ```

3. Görüntü kullanıyorsanız ShadowMount çalışsın ve FC27 kütüphanede görünsün. Başka araçla aynı anda bağlama, açma veya sıkıştırma yapmayın.
4. Payload Manager’dan ELF’i başlatın. **`v0.2.0-beta (KLASOR+GORUNTU)`** bildirimi gelmeli.
5. Tamamlandı bildirimi sonrası oyunda Türkçe spikeri seçip çevrimdışı maç deneyin. Dosyalar tanındığında oyun içinden ayrıca indirmek gerekmez.

Dosyaları PS5’e koymak isterseniz:

```text
/data/pldmgr/payloads/FC27_TR/FC27_TR_KUR.elf
/data/FC27_TR/FC27_TURKCE_SPIKER_PPSA34015_v01.000.004.zip
```

## Oyun klasörü nerede olabilir?

Klasör ismi serbesttir. `etaHEN` oluşturmanız veya oyunu iç diske taşımanız gerekmez. `sce_sys/param.json`, `ampr_emu.index` ve `Data/Ps5` aynı oyun kökünde bulunmalı; kimlik ve sürüm doğrulanır.

- USB0–USB7: disk kökü ve en fazla dört klasör derinliği.
- PS5 `/data`: kök ve en fazla 32 klasör derinliği.
- Örnekler: `SSD/FC27`, `SSD/games/FC27`, `SSD/OnionHEN/games/FC27`, `/data/oyunlarim/FC27`.

Gizli klasörler, sembolik bağlantılar, ayrı bağlı alt dosya sistemleri ve disk sistem klasörleri izlenmez. Her arama kökü en fazla 4.096 dizin / 32.768 girdiyle sınırlıdır. Birden fazla uygun kopyada veya eksik taramada otomatik seçim yapılmaz.

## exFAT / FFPFSC görüntüsü nasıl kuruluyor?

**Diskin exFAT biçimli olması ile oyunun `.exfat` dosyası olması farklıdır.** Açılmış klasör normal klasör yöntemiyle kurulur. Oyun tek görüntü dosyasıysa yeni yöntem onu salt okunur bağlar, kimlik ve indeksini kontrol eder.

Türkçe dosyaları ShadowMount’un gerçekten seçtiği `backports/PPSA34015` klasörüne kurulur. Mevcut backport yoksa yeni konum:

```text
/data/homebrew/backports/PPSA34015/
├── ampr_emu.index
└── Data/Ps5/...
```

Bunu elle oluşturmanız gerekmez. Var olan diğer backport dosyaları, `fakelib`, başlangıç dosyası ve unlock değiştirilmez. Görüntü ve `.vhash` dosyasına yazılmaz. **Game Compressor gerekmez.** Kurulumdan sonra bu ses klasörünü silmeyin; oyun sesleri buradan okur.

İçinde `ampr_assets.index` bulunan AMPR varlık paketleri bu modda kabul edilmez. Bu dosyayı silerek kontrolü atlatmayın.

Mevcut backport indeksi görüntüyle Türkçe boyut alanları dışındaki yerlerde farklıysa kurucu durur. Başka oyunun tüm indeksini kopyalamayın.

## Hedefi elle seçmek

Özellikle klasör yedeği ve görüntü birlikte duruyorsa oynadığınız kaynağı seçin. Ayar dosyası **PS5’te** `/data/FC27_TR/install.conf` konumunda olmalıdır; USB’ye konan ayar okunmaz.

Klasör örneği:

```ini
game=/mnt/usb0/oyunlarim/FC27
```

Görüntü örneği:

```ini
game=/data/etaHEN/games/PPSA34015.ffpfsc
```

Görüntü yolunun ShadowMount’a kayıtlı gerçek kaynakla eşleşmesi gerekir. `/app0`, `/system_ex/app` veya geçici `/mnt/shadowmnt` yolunu seçmeyin. `overlay=` yazmanız gerekmez; hedef gerçek bağlamadan belirlenir.

İsteğe bağlı ZIP seçimi:

```ini
zip=/mnt/usb0/FC27_TR/FC27_TURKCE_SPIKER_PPSA34015_v01.000.004.zip
```

Otomatik ZIP araması sırasıyla `/data/FC27_TR`, `/data`, USB0–USB7’de `/FC27_TR` ve USB köküdür. Otomatik oyun araması önce klasörleri tarar; uygun klasör yoksa ShadowMount’un kayıtlı görüntüsünü sorgular.

Yalnız kontrol için:

```ini
mode=check
```

Kontrol oyun/katman dosyalarını veya yeni hedef klasörlerini oluşturmaz; günlük yazabilir. Görüntüyü geçici salt okunur bağlayıp kendi açtığı bağlamayı bırakabilir. Kurulum için satırı kaldırın veya `mode=install` yapın.

## Hatalar ve sınırlar

Günlük: `/data/FC27_TR/installer.log`. Bu klasör varsa günlük tutulur.

| Mesaj / durum | Çözüm |
| --- | --- |
| Oyun açık | Tamamen kapatın. Kurucu oyunu kendisi kapatmaz. |
| ZIP yok | Dosya adını ve konumu düzeltin; gerekirse `zip=` kullanın. |
| Arama sınırı / birden fazla kopya | `game=` ile hedef seçin. |
| ShadowMount bağlantısı yok | ShadowMount’u başlatın; yerel API’nin varsayılan portta açık olduğunu kontrol edin. |
| Kaynak görüntü eşleşmedi | ShadowMount’un oynattığı fiziksel görüntü dosyasını seçin. |
| Bağlama başka işleme ait | Oyun ve diğer bağlama araçlarını kapatın; kendi hazırladığınız bağlamayı ShadowMount üzerinden ayırıp tekrar deneyin. |
| Yeni klasör hazırlanıyor | ShadowMount’un varsayılan yaklaşık 10 saniyelik beklemesi otomatik uygulanır. Özel kararlılık ayarı 60 saniye ve üzerindeyse kurucu açıklayıcı hatayla durur. |
| İndeks / paket uyumsuz | Kurulum değişiklik yapmadan durur; günlükle birlikte oyun sürümünü bildirin. |
| CRC / SHA256 hatası | Ses ZIP’i farklı veya bozuk; beklenen paketi doğrulayın. |
| Alan yetersiz | Seslerin kurulacağı depolamada yer açın; görüntünün iç boşluğu kullanılmaz. |
| `.tr-stage`, `.tr-previous` veya kilit kaldı | Özellikle `.tr-previous` dosyalarını rastgele silmeyin; günlükle birlikte inceleyin. |
| Dosyalar kuruldu, maç açılmıyor | Genel oyun dosyaları/backport sorunu ayrı incelenmeli. Türkçe kurucu oyun onarım paketi değildir. |

İşlem kesilirse normal hata geri alma mekanizması çalışır; elektrik kesintisinde otomatik kurtarma garantisi yoktur. Diğer uygulamalar aynı kaynağı taşırken veya bağlamasını değiştirirken kurulum yapmayın. ShadowMount beta3 API’si uygulamalar arasında atomik sahiplik kilidi sağlamaz; kurucu kimlik değişirse durur.

FW 13.60’ta Game Compressor için resmî projede uyumluluk değişiklikleri önerilmiştir ([PR #69](https://github.com/juma-sayeh/PS5-Game-Compressor/pull/69)); bunlar bu kurucunun 13.60’ta test edildiği anlamına gelmez. Bu yöntem Game Compressor’a bağlı değildir.

## Hangi dosyalar değişir?

Yalnız bilinen 10 Türkçe varlık ve ilgili AMPR indeks boyutları. Değişecek dosyalar geçici alana çıkarılır, CRC/SHA256 ile doğrulanır; ardından yerleştirilir. Diğer indeks baytları korunur. Mevcut doğru dosyalar tekrar yazılmaz. Hata durumunda önceki hedef dosyaları geri alınır.

Açılmış klasörde dosyalar doğrudan oyun klasörüne; görüntüde ayrı backport klasörüne yazılır. Kurucu FTP kullanmaz. Görüntü için yalnız PS5’in yerel ShadowMount API’siyle haberleşir.

## PC ile manuel alternatif

Native ELF yerine `tools/TR_INDEX_DUZELT.py` kullanılabilir. Bu araç **yerel PC dosyaları** üzerinde çalışır ve PS5'e bağlanmaz.

1. Oyunu tamamen kapatın.
2. ZIP'i açın. `PPSA34015-app0/Data` içeriğini yolları koruyarak mevcut oyun klasörünüzdeki `Data` ile birleştirin. Oyun klasörü içinde ikinci bir `PPSA34015-app0` oluşturmayın.
3. Hedef oyun klasöründeki mevcut `ampr_emu.index` ile aynı oyunun `sce_sys/param.json` dosyasını PC'ye alın.
4. Bu iki dosyayı, ZIP'i ve `TR_INDEX_DUZELT.py` dosyasını aynı PC klasörüne koyun. O klasörde terminal açın. Python 3.9 veya yenisiyle çalıştırın:

   ```sh
   python3 TR_INDEX_DUZELT.py --index ampr_emu.index --param param.json --paket FC27_TURKCE_SPIKER_PPSA34015_v01.000.004.zip --output ampr_emu_TR.index
   ```

   Windows'ta `python3` yerine `python` veya `py` kullanılabilir.

5. Araç ZIP dosyalarını doğrular ve yeni `ampr_emu_TR.index` üretir; orijinal PC indeksini değiştirmez. Hedef oyunun mevcut indeksini gerektiğinde geri dönebileceğiniz bir adla ayırın ve yeni dosyayı oyun klasörüne `ampr_emu.index` adıyla aktarın.
6. Oyunu açıp Türkçe spikerle çevrimdışı maç deneyin.

Başka oyun paketinden alınan tüm indeksi kullanmayın. Düzeltme, kurulumunuzun kendi indeksine uygulanmalıdır.

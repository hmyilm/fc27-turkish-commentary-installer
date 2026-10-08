# Oyun görüntüleri için harici dosya katmanı: teknik inceleme

8 Ekim 2026. Bu belge, exFAT ve FFPFSC oyun görüntülerinin içini değiştirmeden Türkçe verileri ayrı bir ShadowMount+ backport klasöründen sunma olasılığını inceler. Bu belge başlangıçtaki kaynak incelemesini kaydeder. Sonrasında v0.2.0-beta ile FW 13.20 / ShadowMount 1.7beta3 / FFPFSC içindeki exFAT üzerinde kurulum, yeniden bağlama ve Türkçe maç doğrulandı. Güncel kapsam ve sınırlar [test durumunda](TEST_DURUMU.md). Aşağıdaki “henüz denenmedi” ifadeleri ilk inceleme aşamasına aittir.

## Sonuç ve kapsam

ShadowMount+ 1.7beta3 kaynak kodu ile kaydedilmiş FC27/AMPR günlükleri, normal dosyalar kullanan bir oyun görüntüsünün üzerine Türkçe dosyaları ve o oyundan türetilmiş indeksi sunmanın teknik olarak mümkün olabileceğini gösteriyor. Bu bir kaynak incelemesi sonucudur; gerçek konsolda dosya görünürlüğü ve Türkçe maç testi gerekir.

Üç biçim birbirinden ayrılmalıdır:

| Biçim | İnceleme sonucu |
| --- | --- |
| exFAT biçimli SSD üzerindeki açılmış oyun klasörü | Mevcut klasör kurulum yoludur. |
| exFAT veya FFPFSC oyun görüntüsü; oyun dosyaları normal yollarında bulunuyor | ShadowMount+ dosya katmanı için aday. Görüntünün içine yazmadan çalışması beklenebilir; henüz denenmedi. |
| AMPR varlık paketleri (`ampr_assets.index` ve `.pak` birimleri) kullanan oyun | İlk görüntü desteğinde reddedilmeli. Normal dosya katmanı yeterli olmayabilir. |

## ShadowMount+ 1.7beta3: bağlama sırası

İncelenen yerel ShadowMount+ kaynak ağacının kimliği `f0d15ffc46e9237d41cc3555b1cf11362d9a32e0`. Kaydedilmiş `1.7beta3...1.7beta4` karşılaştırmasındaki başlangıç commit'i de bu kimliktir; aşağıdaki sonuçlar beta3 koduna dayanır.

1. `prepare_title_runtime()` önce görüntü kaynağını hazırlar (`prepare_image_source`).
2. Kaynak oyun dizinini `nullfs` ile `/system_ex/app/<TITLE_ID>` konumuna bağlar.
3. Seçilen backport klasörünü aynı konumun üzerine `unionfs` ile bağlar.
4. Oyun süreci bu hazırlıktan sonra başlatılır. Çalışan oyun içindeki `/app0`, oyun verilerinin okunduğu ad alanıdır.

Kaynaklar: [çalışma zamanı hazırlama sırası](https://github.com/drakmor/ShadowMountPlus/blob/f0d15ffc46e9237d41cc3555b1cf11362d9a32e0/src/sm_shellcore_service.c#L410), [nullfs hedefi](https://github.com/drakmor/ShadowMountPlus/blob/f0d15ffc46e9237d41cc3555b1cf11362d9a32e0/src/sm_filesystem.c#L1305), [backport hedefi](https://github.com/drakmor/ShadowMountPlus/blob/f0d15ffc46e9237d41cc3555b1cf11362d9a32e0/src/sm_scan.c#L840), [unionfs seçenekleri](https://github.com/drakmor/ShadowMountPlus/blob/f0d15ffc46e9237d41cc3555b1cf11362d9a32e0/src/sm_filesystem.c#L862).

Bu sıra görüntü dosya sisteminin üstüne uygulanır; FFPFSC'nin alttaki sıkıştırma katmanı veya exFAT'in blok düzeni için ayrıca CAS fiziksel konumları üretmek gerekmemesi beklenir. Bu beklenti, normal dosya okumaları içindir; aşağıdaki AMPR varlık paketi istisnası ayrıca kontrol edilmelidir.

### Backport klasörünün seçilmesi

Beta3, `<scanpath>/backports/<TITLE_ID>/` biçimini kullanır. Oyunun kendi tarama kökü önceliklidir; ardından yapılandırılmış kökler ve dahili geri dönüş konumu kullanılır. `/data/homebrew/backports/<TITLE_ID>/` varsayılan geri dönüş konumudur. Birden fazla ayrı backport klasörü otomatik birleştirilmez; ilk seçilen klasör kullanılır.

Bu nedenle Türkçe dosyalar başka bir backportu gölgede bırakacak rastgele yeni bir klasöre yazılmamalıdır. Kullanılan backport konumu belirlenmeli; mevcut dosyalar korunmalı ve çakışan `ampr_emu.index` özel olarak incelenmelidir.

Önerilen iç yerleşim, seçilen backport klasörü altında oyun köküne göredir:

```text
backports/PPSA34015/
├── ampr_emu.index
└── Data/Ps5/...
```

Araya `PPSA34015-app0` veya `app0` klasörü eklenmez. On Türkçe dosyanın oyun içi yolları korunur. İndeks, kullanıcının görüntüsündeki veya mevcut etkin backportundaki indeksten türetilmelidir; başka bir oyunun/kurulumun tüm indeksi kullanılmamalıdır.

Kaynaklar: [backport seçimi](https://github.com/drakmor/ShadowMountPlus/blob/f0d15ffc46e9237d41cc3555b1cf11362d9a32e0/src/sm_scan.c#L768), [beta3 açıklaması](https://github.com/drakmor/ShadowMountPlus/blob/f0d15ffc46e9237d41cc3555b1cf11362d9a32e0/README.md#L181).

## AMPR dosya yolu ve indeks davranışı

Yerel `ampr_0.3.6.6_index.cpp` kopyasında:

- `CompactFileIndexEntry` yalnız `pathOffset`, `pathLength`, `size`, `mtime` içerir. `pathOffset`, indeksteki yol metnine işaret eder; oyun görüntüsünün fiziksel blok konumu değildir (satır 30).
- İndeks yolu `/app0/ampr_emu.index` olarak tanımlıdır (satır 118–121).
- İndeks normal `ampr_real_sceKernelOpen(..., O_RDONLY, ...)` çağrısıyla açılır (satır 1394).
- `sceKernelOpen_emul()` önce verilen dosya yolunu normal çekirdek çağrısıyla açar. Yalnız bulunamama durumunda indeksin aynı `/app0` ad alanındaki doğru yazımlı yolunu dener (satır 1837).
- Dosya kimliğini çözümleyen `ampr_index_get_entry_view()` yine yol ve boyut döndürür (satır 1973).
- İndeks belleğe alındıktan sonra modül ömrü boyunca sabit kabul edilir (satır 89). Dosyaları ve indeksi oyun çalışırken değiştirmek uygun değildir.

İncelenen kaynak: [AMPR 0.3.6.6 indeks modülü](https://github.com/drakmor/ampr_emu/blob/0.3.6.6/src/ampr_emu_index.cpp). Yerel 0.4.2.1 çalışma zamanı kaynak kopyası eksiktir; bütün okuma uygulamasının kaynak düzeyinde incelendiği iddia edilmez.

Kaydedilmiş FC27 0.4.2.1 tanılama çıktısı da aynı ad alanını doğrular:

- `ampr-full-first.log`, satır 8–15: `/app0/ampr_emu.index` açılıyor ve 502 kayıt yükleniyor.
- Aynı günlük, satır 16: `apr.pack.index.missing path=/app0/ampr_assets.index`.
- Aynı çalıştırmanın dosya tanımlayıcı sayaçlarında `packOpen=0` görülüyor.
- `commands.txt`, satır 23 ve devamında CAS okuma yolları `/app0/Data/Ps5/superbundlelayout/...` biçiminde. Çözümlenen 9.534 APR okuma komutunun dosya yolları bu indeksle eşleştirilmiş.

Bu deliller, kayıt alınan kurulumda CAS okumalarının fiziksel görüntü konumlarıyla çalışan ayrı bir AMPR paketinden gelmediğini destekler. Başka bir kullanıcının veya sonradan yeniden paketlenmiş oyunun aynı biçimde olduğunu kanıtlamaz.

## AMPR varlık paketleri: ilk uygulamada durdurulacak durum

İncelenen `ASSET_PACK_WORKFLOW_EN.md` belgesinde çalışma zamanı seti şöyledir:

| Dosya | İşlev |
| --- | --- |
| `ampr_emu.index` | Kaynak oyun dosyalarının kimlik/yol/boyut indeksi. |
| `ampr_assets.index` | AMPRPAK varlık paketi manifesti; hangi verinin paketli olduğunu tanımlar. |
| `ampr_assets.index.runtime` | Manifestin build ID'sine bağlı çalışma zamanı ayarları. Olmaması derleme varsayılanlarını kullanır; tek başına paket manifesti değildir. |
| Manifestte adı geçen `.pak` dosyaları | Paketlenmiş içerik birimleri. Adları manifestten gelir; tek bir sabit birim adı varsayılmamalıdır. |
| `ampr_assets.index.crc` | Bilgisayardaki doğrulama/açma için yardımcı dosya. Belgede çalışma zamanının bunu yüklemediği açıkça belirtilir. |

Paketleme belgesi hem AMPRPAK4 biçimini hem önceki AMPRPAK3'ten dönüşümü anlatır; yalnız dosya uzantısına bakılarak tek biçim varsayılmamalıdır. PACK olarak işaretlenen asıl dosyalar oyun klasöründen kaldırılabilir; LOOSE dosyalar yerinde kalır. Bu nedenle bir CAS yoluna yeni dosya koymak, o dosyanın paket manifesti üzerinden okunan verinin önüne geçeceğini kanıtlamaz. Yerel kopyada tam paket okuma kodu bulunmadığı için paketli/normal dosya önceliği kesinleştirilmedi.

**İlk uygulama için öneri:** etkin oyun kökünde `ampr_assets.index` varsa görüntüye harici Türkçe katmanı kurmayı durdurun. Kontrol hem temel oyun görünümünü hem seçilecek mevcut backport katmanını kapsamalıdır. Manifest okunamıyorsa da yok saymayın. Manifesti silmeyin veya `.runtime` dosyasını değiştirmeyin; diğer oyun verileri bu sete bağımlı olabilir. Daha sonra paket manifestini ayrıştırıp on hedef yolun tamamının LOOSE olduğunun doğrulanması ayrı bir destek çalışması olabilir.

Sadece `.crc` veya `.runtime` kalıntısı paketli okumanın etkin olduğunu tek başına kanıtlamaz. Her `.pak` uzantılı dosyayı da otomatik AMPR paketi saymayın; asıl karar işareti etkin `ampr_assets.index` manifestidir.

Yerel belge kanıtı: `ASSET_PACK_WORKFLOW_EN.md` satır 188–198 (PACK/LOOSE davranışı), 263–280 (AMPRPAK4), 361–388 (asıl dosyaları kaldırma), 393–417 (dağıtım seti), 455–457 (`.runtime`), 612–620 (önceki biçim dönüşümü). Bu kaynak incelemesi görüntüdeki kernel sıkıştırma katmanını AMPR varlık paketleriyle eşdeğer saymaz.

## `.vhash` ve diğer katmanlar

Game Compressor'ın yerel `gc_api.c` kopyası `.vhash` dosyasını görüntüye ait yan dosya olarak yönetiyor: görüntü yayımlanırken beraber taşınıyor, görüntü içi AMPR/indeks değişiklikleri ardından geçersiz kalan yan dosya kaldırılıyor (satır 5881–5955, 9645, 9821). `.vhash`, oyun klasörü veya `ampr_assets.index` değildir. Tek başına kurulum hedefi seçilemez. Yalnız harici backport klasörü yazılıp görüntü baytları korunuyorsa `.vhash` dosyasını değiştirmek için gerekçe yoktur. Bunun konsol doğrulama davranışı ayrıca denenmedi.

Backport içindeki `fakelib`/`fakelib2` başka bir katmandır: ShadowMount+ bunları oyunun sandbox `common/lib` dizinine bağlar. Türkçe veri dosyaları `Data/Ps5` altında kalmalıdır. Ses kurulumu mevcut AMPR/backport kütüphanelerini değiştirmemelidir. Beta3 seçim sırası backport `fakelib2`, backport `fakelib`, oyun `fakelib2`, oyun `fakelib` biçimindedir; seçilen `fakelib2` yalnız kullanılır. [Kütüphane seçimi](https://github.com/drakmor/ShadowMountPlus/blob/f0d15ffc46e9237d41cc3555b1cf11362d9a32e0/src/sm_fakelib.c#L240).

## Doğrulanması gerekenler

- Mevcut oyun ve backport görünümünden doğru indeksin seçildiği, on Türkçe yol ve boyut alanı dışında indeksin değişmediği.
- Yeni katmanın `/system_ex/app/PPSA34015` görünümünde ve oyun açıldıktan sonra sandbox `/app0` görünümünde aynı dosya/hash değerlerini verdiği.
- Kullanılan AMPR sürümünün yeni indeksi açılışta okuduğu; eski süreç/indeks önbelleğinin kalmadığı.
- Hem exFAT hem FFPFSC kaynak için görüntü hash'inin korunması ve oyunun Türkçe spikerle çevrimdışı maça girmesi.
- Mevcut başka backportun, oyun güncellemesinin veya AMPR varlık paketinin davranışı değiştirmediği.

Farklı paketler, v003, FW 13.60 ve tek başına exFAT için genel çalışma iddiası yoktur. Gerçek konsol sonucunun sınırları ve ilk denemelerde düzeltilen sorunlar [test durumunda](TEST_DURUMU.md) kayıtlıdır.

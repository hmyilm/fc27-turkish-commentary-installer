# PS5 ana ekran başlatıcısı

Bu başlatıcı kurucu ELF'i kendi içinde taşır ve açık yerel ELF yükleyiciye
(`127.0.0.1:9021`) gönderir. Kurucu ayrı bir payload işlemi olarak çalışır;
dosya denetimi, kurulum, geri alma ve PS5 bildirimlerini kurucu yürütür.

PKG Türkçe ses dosyalarını içermez. Kullanıcı kendi Türkçe ses ZIP'ini,
ana kurulum rehberindeki konumlardan birine yerleştirmelidir.

## Durum

PKG sürümü konsol kurulumu ve ana ekran açılışı doğrulanana kadar deneme
sürümüdür. Paket oluşturucunun yapısal kontrolü ve geri çıkarma testi,
konsolda kurulum veya çalıştırma testinin yerine geçmez.

`.003` ve `.004` oyun sürümleri kurucu tarafından kabul edilir. Oyun içi
Türkçe spiker testi şu anda `.004` üzerinde yapıldı.

## Derleme

PS5 payload SDK v0.43, Python 3.9+ ve .NET 10 SDK gerekir. SDK için
`PS5_PAYLOAD_SDK` değişkenini ayarlayın. SDK'nın LLVM bulamadığı sistemlerde
`LLVM_CONFIG` değişkeniyle `llvm-config` dosyasını seçin.

Projenin ana klasöründe çalıştırın. `prepared-app` ve `fresh-output`
klasörlerini her denemede yeni adlarla oluşturun:

```sh
sh ./build.sh
sh ./build-bridge.sh
sh ./launcher/build-auxiliary.sh
python3 launcher/tools/prepare_app.py \
  --bridge launcher/build/bridge.elf \
  --right launcher/build/right_stub.elf \
  --output prepared-app
bash ./launcher/build-pkg.sh prepared-app fresh-output
```

Paketleyici `SvenGDK/LibProsperoPKG` projesinin
`748eabf1b7d17819528cabf367d8e27109d8fce3` revizyonunu kullanır.
`LIBPROSPERO_SOURCE` ile önceden indirilmiş aynı revizyonu,
`DOTNET` ile .NET çalıştırılabilir dosyasını seçebilirsiniz.

`prepare_pkg_library.py` derlemede ayrı bir kitaplık kopyası oluşturur.
Değiştirilecek beş kaynak dosyanın SHA256 değerlerini sabit revizyonla
karşılaştırır. Asıl kitaplık klasörünü değiştirmez. Kopyadaki değişiklikler:

- macOS .NET sağlayıcısında olmayan SHA3-256 için taşınabilir FIPS 202
  uygulaması; SHA-256 ile değiştirme yapılmaz.
- Kitaplığın okuyucusunda, üreticinin tek kayıtla temsil ettiği metadata
  öncesindeki boşluk alanını doğru konuma ilerletme. Paket üretimindeki
  dosya sistemi düzeni değiştirilmez.

SHA3 uygulamasının bağımsız Python `hashlib` karşılaştırması:

```sh
mkdir -p launcher/.build
python3 launcher/tools/make_sha3_vectors.py > launcher/.build/sha3-vectors.json
DOTNET_CLI_TELEMETRY_OPTOUT=1 DOTNET_GENERATE_ASPNET_CERTIFICATE=false \
  "${DOTNET:-dotnet}" run \
  --project launcher/portable-sha3/tests/Sha3Tests.csproj \
  --configuration Release --artifacts-path launcher/.build/sha3-test \
  -- launcher/.build/sha3-vectors.json
```

Oluşturulan paket PS5 `FIH` biçimindedir; köprü PS5 `SELF` biçimine
çevrilir. Bu işlem PS4 paketi üretmez. Payload ELF'i doğrudan uygulama
başlangıç dosyası olarak kullanmak yerine bağımsız köprü kullanılır.

Paket oluşturulduktan sonra tekrar çıkarılır ve çıkan köprü SELF'in tüm
baytları imzalanan kaynak köprüyle karşılaştırılır. Aynı karşılaştırma kendi
`right_stub.c` kaynağımızdan üretilen yardımcı modül için de yapılır;
kitaplığın beraberinde gelen yardımcı modül kullanılmaz. Paket içindeki
`param.json` ve `icon0.png` dosyaları hazırlanan uygulamayla karşılaştırılır.
Başka uygulama dosyası çıkarsa doğrulama başarısız olur.

Sonuç dosyası `fresh-output/package-build-result.json` içinde SHA256
değerleri ve konsol testinin ayrı durumu bulunur. Kitaplık her pakette
rastgele şifreleme tohumu ürettiği için bağımsız derlemelerin PKG SHA256
değerleri farklı olabilir; kaynak ve dosya içeriği karşılaştırması her
derlemede yeniden yapılır.

## Konsolda çalıştırma koşulları

Kurucu PKG'si, çalışır bir PS5 homebrew ortamı ve `9021` portunu dinleyen
yerel ELF yükleyici gerektirir. Ses ZIP'i konsola veya USB'ye önceden
konulmuş olmalıdır. Ana rehberdeki ZIP adı ve konumları aynen geçerlidir.
FC27 kapalıyken ana ekrandaki kurucu simgesi açılır; köprü gömülü kurucuyu
yerel yükleyiciye gönderir. Kurulum sonucunu ELF kurucusunun bildirimleri
ve `/data/FC27_TR/installer.log` kaydı gösterir.

Bu PKG adayının bilgisayardaki paket doğrulamaları geçti. Konsolda
kurulum ve simgeden başlatma henüz doğrulanmadığından çalışan ELF
sürümünün yerine doğrulanmış seçenek olarak sunulmamalıdır.

## Lisans ve kaynaklar

Bu başlatıcı ve SDK'dan uyarlanan başlangıç kodu GPL-3.0-or-later
lisanslıdır. LibProsperoPKG de GPL-3.0-or-later lisanslıdır.

- [PS5 payload SDK başlangıç örneği](https://github.com/ps5-payload-dev/sdk/tree/v0.43/samples/install_app)
- [LibProsperoPKG paket oluşturucu](https://github.com/SvenGDK/LibProsperoPKG/tree/748eabf1b7d17819528cabf367d8e27109d8fce3)
- [Paket oluşturucunun sınırları](https://github.com/SvenGDK/LibProsperoPKG/blob/748eabf1b7d17819528cabf367d8e27109d8fce3/docs/implementation-status.md)
- [SHA3/Keccak algoritma açıklaması](https://keccak.team/keccak_specs_summary.html)

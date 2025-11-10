# UnityVulkanTextureSharingOnLinux

これは Reina_Sakiria が VK_KHR_external_memory_fd を使って、GPU 上のメモリを共有できるのかどうか調べるためのテストレポジトリになります！

それぞれ rust/vulkano での Receiver と Sender があり、Sender は起動すると stdout に pid と fd を出力し、 Receiver は 起動時引数に pid と fd をいれ 特権で起動することで共有を見ることができる。

(以下は片方のみが画像のロード&共有を行いもう片方が共有されたものを使用しているだけなのである。)
![vulkano to vulkano](./README_object/vulkano-to-vulkano.webp)

また、 UnityEditor を Sender として動かす Plugin もあり、適当なカメラにくっつけると共有されている状態がテストできます。

**ただし** Unity Plugin の方は正常に動きませんでした。

![unity to vulkano](./README_object/unity-to-vulkano-demo.mp4)

つまり。

Vulkan 同士の GPU 上メモリの共有の実証実験には成功しましたが、Unity からの共有には成功していません。 Unity 何もわからん。

# PowerStreaming Host

Host Windows para captura de desktop e transmissão local de vídeo H.264.

Pipeline:

`Windows físico -> DXGI Desktop Duplication -> max. 1920x1080 -> AMD AMF/VCE H.264 -> RTP/H.264 -> UDP -> receptor na VM -> janela compartilhada pelo Discord`

## Arquitetura oficial

O Discord ficará dentro da VM, protegida pelo ProtonVPN. O host Windows captura a tela física e envia o vídeo pela interface de rede entre o host e a VM. O cliente receptor renderiza o vídeo em uma janela sem bordas, que será selecionada pelo Discord em **Go Live / Screen Share**.

Uma câmera virtual Windows não faz parte do caminho principal. Ela poderá ser adicionada futuramente apenas se for necessário usar o vídeo como webcam, em vez de compartilhar uma janela.

## Dependências

- Windows 10/11
- Visual Studio 2022 com workload Desktop C++
- Driver AMD instalado e uma GPU compatível com AMF (ex.: RX 5500 XT)
- SDK [AMD AMF](https://github.com/GPUOpen-LibrariesAndSDKs/AMF), extraído localmente

O projeto usa os headers do AMF e o runtime fornecido pelo driver AMD; não há DLL AMF para copiar para o diretório do executável.

## Compilação

Também é possível compilar online pelo workflow `Build PowerStreaming` do GitHub Actions. Ele gera dois artefatos: `PowerStreamingHost-windows` e `PowerStreamingClient-android`. Assim, não é necessário instalar Visual Studio, CMake, Android SDK ou o SDK AMF no computador local.

No Developer PowerShell for VS 2022:

```powershell
cmake -S . -B build -DAMF_ROOT=C:/src/AMF
cmake --build build --config Release
```

`AMF_ROOT` deve apontar para a raiz extraída do SDK, que contém `amf/public/include`.

## Execução

```powershell
.\build\Release\PowerStreamingHost.exe --ip 127.0.0.1 --port 5000 --monitor 0 --resolution 1080 --fps 60 --bitrate-kbps 12000
```

Perfis disponíveis:

- `--resolution 1080`: até 1920x1080, recomendado para qualidade máxima;
- `--resolution 720`: até 1280x720, recomendado para reduzir tráfego e carga da VM.

O payload é RTP estático com payload type dinâmico 96, codec H.264, fragmentação FU-A e timestamp de 90 kHz. O receiver deve escutar UDP/5000 e reconstruir H.264 Annex-B a partir dos pacotes RTP.

### Observações

- `--monitor` é o índice do monitor enumerado pelo DXGI, começando em zero.
- O host sempre reduz a captura para no máximo 1920x1080, mantendo dimensões pares para o codec.
- Para a VM, use o endereço IP do host na interface virtual/host-only. `127.0.0.1` só deve ser usado quando o receptor estiver rodando diretamente no Windows físico.
- O primeiro protótipo faz readback BGRA para a RAM antes do AMF para facilitar validação. Isso é correto funcionalmente, mas a próxima otimização deve usar interop DX11 direto para reduzir latência e uso de CPU.
- Trocar monitor, resolução ou driver pode invalidar a duplicação DXGI; reinicie o host nesses casos.

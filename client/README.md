# PowerStreaming Client

O APK pode ser compilado online pelo workflow `Build PowerStreaming` em `.github/workflows/build.yml`, sem instalar o Android SDK localmente.

Cliente Android para ser executado no LDPlayer dentro da VM.

- escuta RTP/H.264 em UDP/5000;
- reconstrói NALs single-NAL e FU-A;
- decodifica via `MediaCodec` no hardware disponível;
- renderiza em tela cheia, sem controles, para que o Discord compartilhe a janela do LDPlayer.

O host deve enviar para o IP da VM. Dentro do LDPlayer, o endereço de bind é `0.0.0.0:5000`. A rota host→VM deve usar a interface de rede virtual; o ProtonVPN deve cuidar do tráfego do Discord, sem bloquear a rede local entre host e VM.

Escolha o perfil no host antes de iniciar: `--resolution 1080` ou `--resolution 720`. O cliente mantém a janela em tela cheia e preserva a proporção do vídeo recebido.

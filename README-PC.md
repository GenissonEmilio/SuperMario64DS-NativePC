# Super Mario 64 DS PC — uma tela

Projeto independente de adaptação **nativa** para Windows, mantido em [GenissonEmilio/SuperMario64DS-NativePC](https://github.com/GenissonEmilio/SuperMario64DS-NativePC). Parte do port C++ do [tangosdev/sm64ds-decomp](https://github.com/tangosdev/sm64ds-decomp), ramo `release/game-0.9.3.3`, revisão `f18be6bddb638cfe01882dfcf9a99f019ffa0a88`. O código do jogo é compilado para o PC; não usamos melonDS ou outro núcleo de emulação como runtime. Histórico, licença e créditos da base estão preservados. Veja [UPSTREAM.md](UPSTREAM.md) para incorporar atualizações da base ao nosso projeto.

## Adaptações deste projeto

- A aventura usa a cena principal com o HUD original do port integrado: vidas, moedas, estrelas e saúde. O perfil inicial centraliza as moedas e usa a câmera analógica.
- **M** ou o botão **MAPA** abre o mapa sobre a cena. **M**, **FECHAR** ou Esc fecha; voltar a mover o personagem também dispensa o painel. O mapa é uma sobreposição e **não pausa a simulação**.
- O painel que reproduzia a tela inferior no canto fica oculto por padrão. Tab não alterna telas na configuração de PC.
- Mapa tem uma ação própria, separada do Select do DS, e pode ser remapeado em **Settings → Controls → Map**. O botão na tela usa a mesma ação e a transformação de coordenadas do apresentador, inclusive ao redimensionar.
- Mantemos os compositores nativos do upstream para título, seleção de arquivo, seleção de estrela, pausa e outros menus já adaptados.
- Testes da ação e do botão são independentes de ROM; os replays nativos com a ROM rodam localmente, com janela oculta e saves isolados.
- Corrigimos a ordem de inicialização dos locks do NitroFS e a identificação segura da chamada do fader na camada Windows; essas falhas impediam a inicialização ou provocavam uma queda durante o movimento.

WASD move, Espaço pula, X ataca/usa a língua, Ctrl agacha, Shift corre. Segure o botão direito e mova o mouse para girar a câmera analógica; a roda ajusta o zoom. Enter abre a pausa original, com acesso às configurações. As teclas podem ser alteradas no jogo. Os nomes dos menus herdados ainda estão em inglês.

## Compilar e jogar

Requer Windows x64 com as ferramentas **C++ x86** do Visual Studio (o jogo usa ponteiros de 32 bits), CMake/Ninja do mesmo pacote, PowerShell e Python 3. A ROM compatível com essa base é **Europe ASMP, revisão 0**, SHA-256 `ce5829aaa79f06c67140ed1f45cbca738b0a9d0180538c8c1498472d859db032`. A ROM USA não é intercambiável com ela.

```powershell
git clone --branch pc-single-screen https://github.com/GenissonEmilio/SuperMario64DS-NativePC.git
cd SuperMario64DS-NativePC
python -m venv .venv
./.venv/Scripts/python.exe -m pip install -r pc/requirements.txt
```

Baixe o [dsd 0.11.0 oficial](https://github.com/AetiasHax/ds-decomp/releases/tag/v0.11.0), coloque o executável Windows em `tools/bin/dsd.exe`, e execute:

```powershell
./.venv/Scripts/python.exe pc/prepare-rom.py 'C:/caminho/SM64DS-Europe.zip'
./pc/build.ps1
./pc/play.ps1
```

Depois da primeira build, também é possível iniciar dando dois cliques em **Jogar.cmd**, na raiz do projeto.

O importador aceita um `.nds` ou ZIP com uma ROM e mantém o arquivo original intacto. Dados extraídos e intermediários ficam em `extracted/` e `build/`, ignorados pelo Git. Saves desta configuração ficam em `build/user/save/`; faça uma cópia antes de trocar de build. Um `settings.json` já existente ao lado do executável é preservado.

A build usa `PORT_ROM_CLEAN=ON`: tabelas extraídas são carregadas de um blob local, em vez de embutidas no executável. Isso permite preparar um pacote sem dados do jogo. Este repositório não distribui a ROM, os blobs, os assets, saves ou capturas do jogo. O comando de build descobre o Visual Studio instalado, sem exigir uma pasta `2022` específica.

## Verificação

```powershell
./pc/build.ps1 -TestsOnly
python pc/verify-native.py
```

O primeiro comando executa **56 verificações sem ROM**: ação de mapa, clique, bloqueios durante transições, entrada Select, limites de renderização em várias proporções e regressões das duas correções de inicialização. A CI pública roda somente essa parte. O segundo exige sua extração e a build do jogo, dirige o replay interno de entrada e salva evidências privadas em `build/verification/`. Não usa teclas ou cliques do desktop.

Na build local, os quatro replays nativos em Castle Grounds terminaram sem erro: cena parada, movimento, abrir mapa com M e abrir/fechar com M. As **sete verificações de integração passaram**, incluindo a mudança real da posição do personagem e o retorno à mesma imagem após fechar o mapa. Também conferimos o mapa em 16:9 e a execução da cena de título. Isso cobre esses fluxos, sem certificar fases e minigames que não foram jogados.

## Estado e limites

Este projeto está em desenvolvimento, focado na campanha e em seus menus. **Não é uma certificação de toda a campanha nem uma adaptação concluída dos 30 minigames.** Os minigames ainda seguem os layouts e limitações do upstream, incluindo os que dependem de duas superfícies; exigem adaptação própria de jogo e interface. Multiplayer também não foi certificado neste projeto.

A decompilação, os modelos de câmera/física, renderizadores, áudio, salvamento e menus nativos existentes são trabalho do upstream e dos autores listados em [CREDITS.md](CREDITS.md). As adaptações de jogo deste projeto ficam em `port/` e `pc/`, preservando `src/` e a compilação byte a byte do projeto original. Licença MIT original preservada em [LICENSE](LICENSE); ela não concede direitos sobre material da Nintendo.

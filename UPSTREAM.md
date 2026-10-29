# Base original e atualizações

**SuperMario64DSPC é um repositório independente mantido em [GenissonEmilio](https://github.com/GenissonEmilio/SuperMario64DSPC).** O desenvolvimento e as releases deste projeto são geridos aqui. O código deriva do port nativo e da decompilação de [tangosdev/sm64ds-decomp](https://github.com/tangosdev/sm64ds-decomp).

Base inicial das adaptações de uma tela:

- Ramo jogável: `release/game-0.9.3.3`.
- Commit: [`f18be6bddb638cfe01882dfcf9a99f019ffa0a88`](https://github.com/tangosdev/sm64ds-decomp/commit/f18be6bddb638cfe01882dfcf9a99f019ffa0a88).
- Licença MIT e copyright originais: [LICENSE](LICENSE).
- Autores e contribuições da base: [CREDITS.md](CREDITS.md), histórico Git e seção de créditos do README original, preservada abaixo da apresentação deste projeto.

A adaptação de uma tela, o botão e a ação de mapa, o perfil de controles e as correções e ferramentas de PC adicionadas por este projeto estão registrados nos commits posteriores à base. A decompilação e o port nativo original são trabalho dos autores acima.

## Incorporar uma atualização da base

O Git permite trazer atualizações mesmo com o repositório independente no GitHub. `origin` aponta para nosso projeto; `upstream` aponta para a base. Em um clone novo, adicione esse segundo remoto uma vez:

```powershell
git remote add upstream https://github.com/tangosdev/sm64ds-decomp.git
```

Neste checkout de desenvolvimento o remoto já está configurado. Confira com `git remote -v`. Se o clone for raso, obtenha o histórico antes da integração com `git fetch --unshallow upstream`.

Comece com a árvore de trabalho limpa e prepare uma branch de integração:

```powershell
git switch pc-single-screen
git pull --ff-only origin pc-single-screen
git fetch upstream
git switch -c integration/upstream-update
```

Escolha e revise o ramo ou commit **jogável** do upstream. O `main` da decompilação pode ter objetivos diferentes do ramo do jogo. O exemplo abaixo aponta para a base inicial; troque o valor por uma revisão nova que tenha sido avaliada:

```powershell
$baseRef = 'upstream/release/game-0.9.3.3'
git log --oneline HEAD..$baseRef
git merge --no-ff $baseRef
```

Resolva eventuais conflitos preservando as adaptações locais e rode `./pc/build.ps1`, seguido de `python pc/verify-native.py` com os dados locais do jogo. Revise o diff antes de integrar a branch em `pc-single-screen`. Atualize a referência da base neste documento quando a integração for concluída. ROM, blobs, assets extraídos e saves continuam locais e ignorados pelo Git.

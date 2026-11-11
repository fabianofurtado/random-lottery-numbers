#!/usr/bin/env bash
# Inclui apostas da Lotofácil no carrinho do Loterias Online (Caixa).
# Não grava nada em disco, nem temporário: tudo sai em stdout/stderr.
# Uso: rode com -h.
#
# Dependências:
#   bash 4+     (${var,,})
#   curl
#   jq 1.6+     (--args / $ARGS.positional)
#   GNU coreutils: date (%3N e -d @), base64, cat, sleep (fracionário)

set -u
set -f   # sem expansão de caminhos ao separar as dezenas

URL='https://silce.carrinho.caixa.gov.br/loterias-web/carrinho/v1/logado/carrinhos/incluir-aposta/'
SITE='https://www.loteriasonline.caixa.gov.br'
UA='Mozilla/5.0 (X11; Linux x86_64; rv:160.0) Gecko/20100101 Firefox/160.0'
PAUSA=${PAUSA:-0.2}
PRECO=${PRECO:-3.5}
RE_BOT='shieldsquare-response: *([0-9]+)'

uso()
{
  local fd=2 me=${0##*/}

  (( $1 == 0 )) && fd=1
  printf '%s\n' \
    "uso: $me -k subscription_key -t token -c concurso arquivo" \
    '' \
    '  arquivo  um jogo por linha, 15 dezenas (1 a 25) separadas por' \
    '           espaço; linhas em branco e o que vier após # são ignorados' \
    '' \
    '  stdout   jogos incluídos no carrinho' \
    '  stderr   jogos não incluídos, um por linha, com o motivo após #;' \
    '           avisos e resumo como comentário (#)' \
    '' \
    '  O stderr pode ser gravado e usado como entrada de uma nova execução.' \
    '  Use outro nome no 2> da segunda vez: o shell zera o arquivo de' \
    '  saída antes de o script ler a entrada.' \
    '' \
    "    $me -k KEY -t TOKEN -c 3827 jogos.txt \\" \
    '      2> lotofacil_3827_pendentes.log' \
    "    $me -k KEY -t TOKEN_NOVO -c 3827 lotofacil_3827_pendentes.log \\" \
    '      2> lotofacil_3827_pendentes2.log' \
    '' \
    '  ambiente PAUSA  segundos entre envios (padrão 0.2)' \
    '           PRECO  valor da aposta simples (padrão 3.5)' >&"$fd"
  exit "$1"
}

KEY= TOKEN= CONC=
while getopts 'hk:t:c:' opt; do
  case $opt in
    h) uso 0 ;;
    k) KEY=$OPTARG ;;
    t) TOKEN=${OPTARG#Bearer } ;;
    c) CONC=$OPTARG ;;
    *) uso 2 ;;
  esac
done
shift $(( OPTIND - 1 ))
ARQ=${1:-}

[[ -n $KEY && -n $TOKEN && $CONC =~ ^[0-9]+$ && -r $ARQ ]] || uso 2
command -v jq >/dev/null || { echo "# erro: jq não encontrado" >&2; exit 1; }

# Comentário no stderr
nota() { printf '# %s\n' "$*" >&2; }

# Validade do token: campo exp do payload do JWT (base64url)
p=${TOKEN#*.}; p=${p%%.*}
p=${p//-/+}; p=${p//_//}
while (( ${#p} % 4 )); do p+='='; done
EXP=$(printf '%s' "$p" | base64 -d 2>/dev/null | jq -r '.exp // empty' 2>/dev/null)
if [[ $EXP =~ ^[0-9]+$ ]]; then
  quando=$(date -d "@$EXP" '+%d/%m/%Y %H:%M:%S')
  resta=$(( EXP - $(date +%s) ))
  if (( resta > 0 )); then
    nota "$(printf 'token válido até %s (restam %dm%02ds)' \
      "$quando" $(( resta / 60 )) $(( resta % 60 )))"
  else
    nota "token expirou em $quando"
  fi
else
  EXP=
  nota "aviso: não consegui ler a validade do token"
fi

# stdout: jogo incluído
incluido() { printf 'combinacao  %5d (%s): ok\n' "$ln" "$dz"; }

# stderr: jogo não incluído, no formato de entrada, com o motivo após #
pendente() { printf '%s  # combinacao  %5d: %s\n' "$dz" "$ln" "$1" >&2; }

# stderr: jogo atual como pendente e o resto do arquivo como está
parar()
{
  pendente "$1, parando"
  parou=$ln
  cat <&3 >&2
}

nok=0 nerr=0 ninv=0 ln=0 parou=
while IFS= read -r linha <&3; do
  (( ++ln ))
  linha=${linha%$'\r'}
  linha=${linha%%#*}
  nums=( $linha )
  (( ${#nums[@]} )) || continue

  # 15 dezenas distintas entre 1 e 25; os índices de seen saem ordenados
  seen=(); bad=
  for n in "${nums[@]}"; do
    if ! [[ $n =~ ^[0-9]{1,2}$ ]]; then bad=1; break; fi
    v=$(( 10#$n ))
    if (( v < 1 || v > 25 || ${seen[v]:-0} )); then bad=1; break; fi
    seen[v]=1
  done
  dez=( "${!seen[@]}" )
  if [[ -n $bad ]] || (( ${#dez[@]} != 15 )); then
    (( ++ninv ))
    printf '# combinacao  %5d (%s): inválida, ignorada\n' \
      "$ln" "${nums[*]}" >&2
    continue
  fi
  printf -v dz '%02d ' "${dez[@]}"; dz=${dz% }

  if [[ -n $EXP ]] && (( $(date +%s) >= EXP - 15 )); then
    parar 'token expirado'
    break
  fi

  # -a: saída só ASCII (á vira \u00e1), sem risco de encoding
  json=$(jq -nac \
    --argjson conc "$CONC" \
    --argjson valor "$PRECO" \
    --arg data "$(date -u +%Y-%m-%dT%H:%M:%S.%3NZ)" '{
      modalidade: "LOTOFACIL",
      descricaoModalidade: "Lotof\u00e1cil",
      classe: "lotofacil",
      nomeJogo: "lotofacil",
      tituloJogo: "Lotof\u00e1cil",
      tituloHeaderJogo: "titulo-lotofacil",
      geraEspelho: false,
      cssTabCarrinhoJogo: "tab-lotofacil",
      numerosSelecionados: $ARGS.positional,
      quantidadeTeimosinhas: 0,
      concursos: [$conc],
      concursoAlvo: $conc,
      tipoConcurso: {valor: "1", descricao: "Normal"},
      dataInclusao: $data,
      indicadorSurpresinha: {valor: 1},
      indicadorCotaBolao: false,
      valor: $valor
    }' --args "${dez[@]}")

  # Headers e corpo vêm juntos no stdout do curl; o código, na última linha.
  # Repete só em rate limit, erro do servidor ou falha de conexão.
  for tentativa in 1 2 3; do
    resp=$(curl -s --compressed -D - -w '\n%{http_code}' \
      -H "User-Agent: $UA" \
      -H 'Accept: application/json, text/plain, */*' \
      -H 'Content-Type: application/json;charset=utf-8' \
      -H "Referer: $SITE/" \
      -H "Origin: $SITE" \
      -H 'Subcanal: 1' \
      -H "ocp-apim-subscription-key: $KEY" \
      -H "Authorization: Bearer $TOKEN" \
      --data-binary "$json" "$URL")
    code=${resp##*$'\n'}
    resp=${resp%$'\n'*}
    [[ $code == 429 || $code == 5?? || $code == 000 ]] || break
    (( tentativa < 3 )) && sleep $(( tentativa * 5 ))
  done
  hdrs=${resp%%$'\r\n\r\n'*}
  body=${resp#*$'\r\n\r\n'}

  if [[ ${hdrs,,} =~ $RE_BOT ]] && (( BASH_REMATCH[1] != 0 )); then
    parar "anti-bot reagiu (shieldsquare-response: ${BASH_REMATCH[1]})"
    break
  fi

  case $code in
    2??)
      (( ++nok ))
      incluido
      ;;
    401|403)
      parar "HTTP $code (token inválido ou expirado?)"
      break
      ;;
    000)
      (( ++nerr ))
      pendente 'falha de conexão'
      ;;
    *)
      (( ++nerr ))
      body=${body:0:200}; body=${body//[$'\r\n']/ }
      pendente "HTTP $code${body:+ $body}"
      ;;
  esac

  sleep "$PAUSA"
done 3< "$ARQ"

[[ -n $parou ]] && nota "interrompido na combinacao $parou"
nota "incluídos: $nok, com erro: $nerr, inválidos: $ninv"
[[ -z $parou ]] && (( nerr == 0 ))

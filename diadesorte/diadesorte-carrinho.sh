#!/usr/bin/env bash
################################################################################
# Inclui apostas do Dia de Sorte no carrinho do Loterias Online (Caixa).
# Não grava nada em disco, nem temporário: tudo sai em stdout/stderr.
# Uso: rode com -h.
#
# Dependências:
#   bash 4+     (${var,,} e ${var^^})
#   curl
#   jq 1.6+     (--args / $ARGS.positional)
#   GNU coreutils: date (%3N e -d @), base64, cat, sleep (fracionário)
################################################################################
set -u
set -f   # sem expansão de caminhos ao separar as dezenas

URL='https://silce.carrinho.caixa.gov.br/loterias-web/carrinho/v1/logado/carrinhos/incluir-aposta/'
SITE='https://www.loteriasonline.caixa.gov.br'
UA='Mozilla/5.0 (X11; Linux x86_64; rv:160.0) Gecko/20100101 Firefox/160.0'
PAUSA=${PAUSA:-0.2}
PRECO=${PRECO:-2.5}
RE_BOT='shieldsquare-response: *([0-9]+)'

MES_NOME=( '' Janeiro Fevereiro Marco Abril Maio Junho Julho Agosto
           Setembro Outubro Novembro Dezembro )
MES_ABREV=( '' JAN FEV MAR ABR MAI JUN JUL AGO SET OUT NOV DEZ )

uso()
{
  local me=${0##*/}

  printf '%s\n' \
    '' \
    'uso:' \
    '  '$me' -k subscription_key -t token -c concurso [-m mes] arquivo' \
    '' \
    '  arquivo: um jogo por linha: 7 dezenas (1 a 31) separadas por espaço,' \
    '           seguidas opcionalmente do mês de sorte; linhas em branco' \
    '           e o que vier após # são ignorados' \
    '  -m mes   mês de sorte dos jogos sem mês na linha' \
    '' \
    '  mês: número (1 a 12) ou abreviação (JAN FEV MAR ABR MAI JUN JUL AGO' \
    '                                      SET OUT NOV DEZ)' \
    '' \
    '  stdout   jogos incluídos no carrinho' \
    '  stderr   jogos não incluídos, um por linha (já com o mês), com o' \
    '           motivo após "# [ERRO]"; avisos e resumo como comentário (#)' \
    '' \
    '  O stderr pode ser gravado e usado como entrada de uma nova execução.' \
    '  Use outro nome no 2> da segunda vez: o shell zera o arquivo de' \
    '  saída antes de o script ler a entrada.' \
    '' \
    '    '$me' -k KEY -t TOKEN -c 1346 -m MAR jogos.txt \' \
    '      2> diadesorte_1346_pendentes.log' \
    '    '$me' -k KEY -t TOKEN_NOVO -c 1346 diadesorte_1346_pendentes.log \' \
    '      2> diadesorte_1346_pendentes2.log' \
    '' \
    '  ambiente PAUSA  segundos entre envios (padrão 0.2)' \
    '           PRECO  valor da aposta simples (padrão 2.5)'
  exit 0
}

# Erro fatal: "[ERRO] mensagem" no stderr e sai com o código $1
erro()
{
  local rc=$1

  shift
  printf '[ERRO] %s\n' "$*" >&2
  exit "$rc"
}

# Erro nos parâmetros: sai com 2
erro_uso() { erro 2 "$* (veja -h)"; }

# Comentário no stderr
nota() { printf '# %s\n' "$*" >&2; }

# Mês (1 a 12 ou abreviação) em $1 -> número em $mes (vazio se inválido)
ler_mes()
{
  local m=${1^^} i

  mes=
  if [[ $m =~ ^[0-9]{1,2}$ ]]; then
    (( 10#$m >= 1 && 10#$m <= 12 )) && mes=$(( 10#$m ))
    return
  fi
  for i in {1..12}; do
    [[ $m == "${MES_ABREV[i]}" ]] && { mes=$i; return; }
  done
}

KEY= TOKEN= CONC= MES_PADRAO=
while getopts ':hk:t:c:m:' opt; do
  case $opt in
    h) uso ;;
    k) KEY=$OPTARG ;;
    t) TOKEN=${OPTARG#Bearer } ;;
    c) CONC=$OPTARG ;;
    m)
      ler_mes "$OPTARG"
      [[ -n $mes ]] || erro_uso "mês inválido: $OPTARG"
      MES_PADRAO=$mes
      ;;
    :) erro_uso "falta o valor de -$OPTARG" ;;
    *) erro_uso "opção inválida: -$OPTARG" ;;
  esac
done
shift $(( OPTIND - 1 ))
(( $# > 1 )) && erro_uso "argumento a mais: $2"
ARQ=${1:-}

[[ -n $KEY ]]   || erro_uso 'falta a subscription key (-k)'
[[ -n $TOKEN ]] || erro_uso 'falta o token (-t)'
[[ -n $CONC ]]  || erro_uso 'falta o número do concurso (-c)'
[[ $CONC =~ ^[0-9]+$ ]] || erro_uso "concurso inválido: $CONC"
[[ -n $ARQ ]]   || erro_uso 'falta o arquivo de jogos'
[[ -r $ARQ && ! -d $ARQ ]] || erro 1 "não consigo ler o arquivo: $ARQ"
command -v jq >/dev/null || erro 1 'jq não encontrado'

# Validade do token: campo exp do payload do JWT (base64url)
p=${TOKEN#*.}; p=${p%%.*}
p=${p//-/+}; p=${p//_//}
while (( ${#p} % 4 )); do p+='='; done
EXP=$(printf '%s' "$p" | base64 -d 2>/dev/null | jq -r '.exp // empty' 2>/dev/null)
if [[ $EXP =~ ^[0-9]+$ ]]; then
  quando=$(date -d "@$EXP" '+%d/%m/%Y %H:%M:%S')
  resta=$(( EXP - $(date +%s) ))
  (( resta > 0 )) || erro 1 "token expirou em $quando"
  nota "$(printf 'token válido até %s (restam %dm%02ds)' \
    "$quando" $(( resta / 60 )) $(( resta % 60 )))"
else
  EXP=
  nota '[AVISO] não consegui ler a validade do token'
fi

# stdout: jogo incluído
incluido() { printf 'combinacao  %5d (%s): ok\n' "$ln" "$dz"; }

# stderr: jogo não incluído, no formato de entrada, com o motivo após #
pendente()
{
  printf '%s  # [ERRO] combinacao  %5d: %s\n' "$dz" "$ln" "$1" >&2
}

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

  # 7 dezenas distintas entre 1 e 31, mais o mês opcional;
  # os índices de seen saem ordenados
  seen=(); bad=; mes=$MES_PADRAO
  case ${#nums[@]} in
    7) [[ -n $mes ]] || bad='sem mês, use -m' ;;
    8) ler_mes "${nums[7]}"; [[ -n $mes ]] || bad='mês inválido' ;;
    *) bad='precisa de 7 dezenas' ;;
  esac
  if [[ -z $bad ]]; then
    for n in "${nums[@]:0:7}"; do
      if ! [[ $n =~ ^[0-9]{1,2}$ ]]; then bad='dezena inválida'; break; fi
      v=$(( 10#$n ))
      if (( v < 1 || v > 31 )); then bad='dezena fora de 1 a 31'; break; fi
      if (( ${seen[v]:-0} )); then bad='dezena repetida'; break; fi
      seen[v]=1
    done
  fi
  if [[ -n $bad ]]; then
    (( ++ninv ))
    printf '# [ERRO] combinacao  %5d (%s): inválida (%s), ignorada\n' \
      "$ln" "${nums[*]}" "$bad" >&2
    continue
  fi
  dez=( "${!seen[@]}" )
  printf -v dz '%02d ' "${dez[@]}"
  dz+=${MES_ABREV[mes]}

  if [[ -n $EXP ]] && (( $(date +%s) >= EXP - 15 )); then
    parar 'token expirado'
    break
  fi

  json=$(jq -nac \
    --argjson conc "$CONC" \
    --argjson valor "$PRECO" \
    --argjson mes "$mes" \
    --arg mesnome "${MES_NOME[mes]}" \
    --arg mesabrev "${MES_ABREV[mes]}" \
    --arg data "$(date -u +%Y-%m-%dT%H:%M:%S.%3NZ)" '{
      modalidade: "DIA_DE_SORTE",
      descricaoModalidade: "Dia de Sorte",
      classe: "dia-de-sorte",
      nomeJogo: "dia-de-sorte",
      tituloJogo: "Dia de Sorte",
      tituloHeaderJogo: "titulo-dia-de-sorte",
      geraEspelho: false,
      cssTabCarrinhoJogo: "tab-dia-de-sorte",
      numerosSelecionados: $ARGS.positional,
      quantidadeTeimosinhas: 0,
      concursos: [$conc],
      concursoAlvo: $conc,
      tipoConcurso: {valor: "1", descricao: "Normal"},
      dataInclusao: $data,
      indicadorSurpresinha: {valor: 1},
      indicadorCotaBolao: false,
      mesDeSorte: {
        numero: $mes,
        nome: $mesnome,
        abreviacao: $mesabrev,
        className: "MesDto"
      },
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

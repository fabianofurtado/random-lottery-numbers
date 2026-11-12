#!/usr/bin/env bash
################################################################################
#   Dia de Sorte: consulta a API da Caixa e acrescenta ao .txt os concursos
#   novos (mantem os ultimos MAX_CONCURSOS, mais novo no fim). Regenera o HTML
#   se houver concurso novo ou se ele nao existir.
#
# Uso: ./diadesorte.sh [-o] [dds_results.txt] [index.html]
#
#   -o: modo offline: nao consulta a API e so regenera o HTML a partir do .txt.
################################################################################
set -euo pipefail

readonly URL_API='https://servicebus3.caixa.gov.br/portaldeloterias/api/diadesorte/'
readonly MAX_CONCURSOS=20
readonly TXT_SEPARATOR=' '
readonly TMSTAMP=$(/usr/bin/date '+%Y-%m-%d %H:%M:%S')

erro() {
  echo "Erro: $*" >&2
  exit 1
}

OFFLINE=0
case ${1:-} in
  -o) OFFLINE=1; shift ;;
  -*) erro "opção inválida: $1" ;;
esac

TXT=${1:-dds_results.txt}
OUT=${2:-index.html}

if [[ -e $TXT && ! -r $TXT ]]; then
  erro "não foi possível ler '$TXT'"
fi

cores=(ff0000 ff3300 ff6600 ff9900 ffcc00 ffff00 ccff00 99ff00 66ff00 33ff00
       00ff00 00ff33 00ff66 00ff99 00ffcc 00ffff 00ccff 0099ff 0066ff 0033ff
       0000ff 3300ff 6600ff 9900ff cc00ff ff00ff ff00cc ff0099 ff0066 ff0033 ff001a)

MES_NOME=(x Jan Fev Mar Abr Mai Jun Jul Ago Set Out Nov Dez)
declare -A MES_NUM=()
for ((i = 1; i <= 12; i++)); do
  MES_NUM[${MES_NOME[i]}]=$i
done

TMP_HTML=''

declare -A s_rep=() s_par=() s_impar=() s_pri=() s_seqs=() s_seqns=() s_mes=()
declare -a ant=() ocorr=() ult_pos=()
for ((i = 1; i <= 31; i++)); do
  ant[i]=0
  ocorr[i]=0
  ult_pos[i]=0
done
n_total=0
soma_total=0
rep_total=0
oc_total=0
oc_sq=0
at_total=0
at_cnt=0

html_add() {
  local trecho
  IFS= read -r -d '' trecho || true
  TMP_HTML+=$trecho
}

busca() {
  local ret resp codigo tentativa

  for tentativa in 1 2 3 4; do
    ret=0
    resp=$(/usr/bin/curl -sS -L --http2 -A '' -H 'Accept: application/json' \
      --connect-timeout 4 --max-time 8 \
      -w $'\x1e%{http_code}' "$URL_API${1:-}") || ret=$?
    if [[ $ret -eq 0 ]]; then
      codigo="${resp##*$'\x1e'}"
      resp="${resp%$'\x1e'*}"
      if [[ $codigo -eq 200 ]]; then break; fi
    else
      codigo=000
      resp=''
    fi
    if (( tentativa < 4 )); then /usr/bin/sleep 12; fi
  done
  [[ $codigo == 200 ]] || erro "API retornou HTTP $codigo (concurso ${1:-último})"
  LINHA=$(/usr/bin/jq -er --arg sep "$TXT_SEPARATOR" '
    def meses: {"jan":"Jan","fev":"Fev","mar":"Mar","abr":"Abr","mai":"Mai","jun":"Jun",
                "jul":"Jul","ago":"Ago","set":"Set","out":"Out","nov":"Nov","dez":"Dez"};
    (.nomeTimeCoracaoMesSorte
      | if type == "string" then meses[ascii_downcase | .[0:3]] else null end) as $mes
    | if (.numero | type) == "number"
         and (.dataApuracao | type) == "string"
         and (.listaDezenas | length) == 7
         and $mes != null
      then [.numero, .dataApuracao] + (.listaDezenas | sort_by(tonumber)) + [$mes] | join($sep)
      else empty end' <<< "${resp}") || erro "JSON inválido (concurso ${1:-último})"
}

isqrt() {
  local n=$1 x y
  if (( n < 2 )); then ISQRT=$n; return; fi
  x=$n
  y=$(( (x + 1) / 2 ))
  while (( y < x )); do
    x=$y
    y=$(( (x + n / x) / 2 ))
  done
  ISQRT=$x
}

estat_linha() {
  local rotulo=$1
  local -n cnt=$2
  local total=${3:-$n_total}
  local -a ch=("${!cnt[@]}")
  local n=${#ch[@]} a b t ct cb p pct nome

  for ((a = 1; a < n; a++)); do
    t=${ch[a]}
    ct=${cnt[$t]}
    for ((b = a - 1; b >= 0; b--)); do
      cb=${cnt[${ch[b]}]}
      if (( cb < ct || (cb == ct && 10#${ch[b]} > 10#$t) )); then
        ch[b + 1]=${ch[b]}
      else
        break
      fi
    done
    ch[b + 1]=$t
  done

  TMP_HTML+="          <tr><th scope=\"row\">$rotulo</th>"
  for t in "${ch[@]}"; do
    ct=${cnt[$t]}
    nome=$t
    if [[ ${4:-} == mes ]]; then nome=${MES_NOME[t]}; fi
    p=$(( (ct * 20000 + total) / (2 * total) ))
    printf -v pct '%d.%02d' $((p / 100)) $((p % 100))
    TMP_HTML+="<td><b>$nome</b> = $pct% ($ct)</td>"
  done
  TMP_HTML+="</tr>"$'\n'
}

LINHAS=()
if [[ -r $TXT ]]; then
  while IFS= read -r linha || [[ -n $linha ]]; do
    linha=${linha%$'\r'}
    if [[ -n $linha ]]; then LINHAS+=("$linha"); fi
  done < "$TXT"
fi

novos=()
if (( OFFLINE )); then
  (( ${#LINHAS[@]} > 0 )) || erro "TXT vazio ou inexistente"
else
  for cmd in /usr/bin/curl /usr/bin/jq; do
    command -v "$cmd" > /dev/null || erro "'$cmd' não encontrado"
  done
  busca
  ultima=$LINHA
  recente=${ultima%%"$TXT_SEPARATOR"*}
  inicio=$(( recente - MAX_CONCURSOS + 1 ))
  if (( ${#LINHAS[@]} > 0 )); then
    anterior=${LINHAS[-1]%%"$TXT_SEPARATOR"*}
    if (( anterior + 1 > inicio )); then inicio=$(( anterior + 1 )); fi
  fi
  if (( inicio < 1 )); then inicio=1; fi
  for ((n = inicio; n < recente; n++)); do
    busca "$n"
    novos+=("$LINHA")
    /usr/bin/sleep 2
  done
  if (( recente >= inicio )); then novos+=("$ultima"); fi
fi

todas=("${LINHAS[@]}" "${novos[@]}")
mudou=0
if (( ${#novos[@]} > 0 || ${#todas[@]} > MAX_CONCURSOS )); then mudou=1; fi
if (( ${#todas[@]} > MAX_CONCURSOS )); then
  todas=("${todas[@]: -MAX_CONCURSOS}")
fi

if (( ! OFFLINE && ! mudou )) && [[ -e $OUT ]]; then exit 0; fi

html_add <<'EOF'
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Dia de Sorte - Últimos Resultados</title>
  <style>
    :root {
      --texto: #333;
      --borda: #ccc;
      --hover: #d8d8d8;
      --cab-1: #ebf3fc;
      --cab-2: #dce9f9;
    }

    * { box-sizing: border-box; }

    body {
      margin: 0;
      padding: 6px 2vw;
      min-width: max-content;
      font: 12px/1.3 "Trebuchet MS", "Lucida Sans", Arial, sans-serif;
      font-size: clamp(12px, 1.05vw, 48px);
      color: var(--texto);
    }

    header { display: flex; align-items: center; gap: 12px; flex-wrap: wrap; }
    header img { max-width: 100%; height: auto; }
    h1 { margin: 8px 0; font-size: 1.6rem; }
    h2 { font-size: 1.2rem; }
    header small { font-size: 0.85rem; color: #777; }
    header small strong { font-weight: 700; color: #333; }

    .resultados {
      border-collapse: separate;
      border-spacing: 0;
      width: 100%;
      white-space: nowrap;
      font-variant-numeric: tabular-nums;
    }

    .resultados th,
    .resultados td {
      padding: 0.1em 0.25em;
      text-align: center;
      border-right: 1px solid var(--borda);
      border-bottom: 1px solid var(--borda);
    }

    .resultados th:first-child,
    .resultados td:first-child { border-left: 1px solid var(--borda); }

    .resultados th:nth-child(n+3):nth-child(-n+33) { width: 2.2em; }

    .resultados th:nth-child(35),
    .resultados td:nth-child(35) { border-left: 2px solid #999; }

    .resultados thead th {
      position: sticky;
      top: 0;
      z-index: 1;
      border-top: 1px solid var(--borda);
      background: linear-gradient(to bottom, var(--cab-1), var(--cab-2));
      text-shadow: 0 1px 0 rgba(255, 255, 255, .5);
    }

    .resultados tbody tr:hover { background: var(--hover); }

    .resultados tfoot th { text-align: left; background: var(--cab-2); }
    .resultados tfoot tr:first-child > * { border-top: 2px solid #999; }

    .estat { border-collapse: collapse; font-variant-numeric: tabular-nums; }
    .estat th { text-align: left; padding: 1px 12px 1px 0; }
    .estat td { padding: 1px 16px 1px 0; white-space: nowrap; }
    .dica { cursor: help; border-bottom: 1px dotted #777; }
EOF

for i in "${!cores[@]}"; do
  TMP_HTML+="    .b$((i + 1)) { background-color: #${cores[i]}; }"$'\n'
done

html_add <<EOF
  </style>
</head>
<body>
  <header>
    <img src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAALwAAAAiCAYAAADlJbOYAAAAGXRFWHRTb2Z0d2FyZQBBZG9iZSBJbWFnZVJlYWR5ccllPAAAA4FpVFh0WE1MOmNvbS5hZG9iZS54bXAAAAAAADw/eHBhY2tldCBiZWdpbj0i77u/IiBpZD0iVzVNME1wQ2VoaUh6cmVTek5UY3prYzlkIj8+IDx4OnhtcG1ldGEgeG1sbnM6eD0iYWRvYmU6bnM6bWV0YS8iIHg6eG1wdGs9IkFkb2JlIFhNUCBDb3JlIDUuNi1jMTQwIDc5LjE2MDQ1MSwgMjAxNy8wNS8wNi0wMTowODoyMSAgICAgICAgIj4gPHJkZjpSREYgeG1sbnM6cmRmPSJodHRwOi8vd3d3LnczLm9yZy8xOTk5LzAyLzIyLXJkZi1zeW50YXgtbnMjIj4gPHJkZjpEZXNjcmlwdGlvbiByZGY6YWJvdXQ9IiIgeG1sbnM6eG1wTU09Imh0dHA6Ly9ucy5hZG9iZS5jb20veGFwLzEuMC9tbS8iIHhtbG5zOnN0UmVmPSJodHRwOi8vbnMuYWRvYmUuY29tL3hhcC8xLjAvc1R5cGUvUmVzb3VyY2VSZWYjIiB4bWxuczp4bXA9Imh0dHA6Ly9ucy5hZG9iZS5jb20veGFwLzEuMC8iIHhtcE1NOk9yaWdpbmFsRG9jdW1lbnRJRD0ieG1wLmRpZDpmMzU4Nzg0Yi03MmE4LTQ1ZjctYWVkMy1lMThhYzNjNTQ0ZTUiIHhtcE1NOkRvY3VtZW50SUQ9InhtcC5kaWQ6ODNBMjc2NDg1NkZFMTFFODkzQTVGMTJCNjJEMTMyM0IiIHhtcE1NOkluc3RhbmNlSUQ9InhtcC5paWQ6ODNBMjc2NDc1NkZFMTFFODkzQTVGMTJCNjJEMTMyM0IiIHhtcDpDcmVhdG9yVG9vbD0iQWRvYmUgUGhvdG9zaG9wIENDIChNYWNpbnRvc2gpIj4gPHhtcE1NOkRlcml2ZWRGcm9tIHN0UmVmOmluc3RhbmNlSUQ9InhtcC5paWQ6NGZjZGQyNWYtMWRkNS00ZjI0LTgyMjctM2E4OWE1NThlZjY3IiBzdFJlZjpkb2N1bWVudElEPSJhZG9iZTpkb2NpZDpwaG90b3Nob3A6MjIwODRkOGQtOWFlZi03MjQzLWI5MjktMzc0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" alt="Dia de Sorte" loading="lazy">
    <h1>Últimos Resultados</h1>
    <small>Atualizado em <strong>${TMSTAMP}</strong></small>
    <a href="#estatisticas">Estatísticas</a>
  </header>
  <main>
    <table class="resultados">
      <thead>
        <tr>
          <th scope="col">Conc</th>
          <th scope="col">Data</th>
EOF

for ((i = 1; i <= 31; i++)); do
  TMP_HTML+="          <th scope=\"col\">${i}º</th>"$'\n'
done

html_add <<'EOF'
          <th scope="col" title="Mês de Sorte">Mês</th>
          <th scope="col" title="Dezenas repetidas do concurso anterior (o primeiro da amostra é sempre 0)">Rep</th>
          <th scope="col" title="Soma das dezenas">Som</th>
          <th scope="col" title="Quantidade de pares">Par</th>
          <th scope="col" title="Quantidade de ímpares">Imp</th>
          <th scope="col" title="Quantidade de primos">Pri</th>
          <th scope="col" title="Maior sequência de dezenas sorteadas">SeqS</th>
          <th scope="col" title="Maior sequência de dezenas não sorteadas">SeqNS</th>
        </tr>
      </thead>
      <tbody>
EOF

for linha in "${todas[@]}"; do
  IFS="$TXT_SEPARATOR" read -r -a c <<< "$linha"
  (( ${#c[@]} >= 10 )) || continue
  mes=${MES_NUM[${c[9]}]:-0}
  (( mes > 0 )) || continue

  row="        <tr><td>${c[0]}</td><td>${c[1]}</td>"
  k=2 soma=0 par=0 pri=0 rep=0
  seqs=0 seqns=0 run_s=0 run_ns=0

  for ((i = 1; i <= 31; i++)); do
    if (( k <= 8 )) && (( i == 10#${c[k]} )); then
      row+="<td>$i</td>"
      (( k += 1 ))
      (( soma += i ))
      if (( i % 2 == 0 )); then (( par += 1 )); fi
      case $i in 2|3|5|7|11|13|17|19|23|29|31) (( pri += 1 )) ;; esac
      (( run_s += 1 ))
      run_ns=0
      if (( run_s > seqs )); then seqs=$run_s; fi
      if (( ant[i] )); then (( rep += 1 )); fi
      ant[i]=1
      (( ocorr[i] += 1 ))
      ult_pos[i]=$((n_total + 1))
    else
      row+="<td class=\"b$i\"></td>"
      ant[i]=0
      (( run_ns += 1 ))
      run_s=0
      if (( run_ns > seqns )); then seqns=$run_ns; fi
    fi
  done

  impar=$((7 - par))

  TMP_HTML+="$row<td>${c[9]}</td><td>$rep</td><td>$soma</td><td>$par</td><td>$impar</td><td>$pri</td><td>$seqs</td><td>$seqns</td></tr>"$'\n'

  (( n_total += 1 ))
  (( soma_total += soma ))
  if (( n_total > 1 )); then
    rep_total=$((rep_total + rep))
    (( s_rep[$rep] += 1 ))
  fi
  (( s_par[$par] += 1 ))
  (( s_impar[$impar] += 1 ))
  (( s_pri[$pri] += 1 ))
  (( s_seqs[$seqs] += 1 ))
  (( s_seqns[$seqns] += 1 ))
  (( s_mes[$mes] += 1 ))
done

html_add <<'EOF'
      </tbody>
      <tfoot>
EOF

tfoot_u="        <tr><th colspan=\"2\" scope=\"row\" title=\"Concursos desde que a dezena foi sorteada (0 = saiu no último)\">Último sorteio</th>"
tfoot_o="        <tr><th colspan=\"2\" scope=\"row\" title=\"Vezes que a dezena foi sorteada na amostra\">Ocorrência</th>"
for ((i = 1; i <= 31; i++)); do
  if (( ult_pos[i] > 0 )); then
    tfoot_u+="<td>$((n_total - ult_pos[i]))</td>"
    at_total=$((at_total + n_total - ult_pos[i]))
    at_cnt=$((at_cnt + 1))
  else
    tfoot_u+="<td>-</td>"
  fi
  tfoot_o+="<td>${ocorr[i]}</td>"
  oc_total=$((oc_total + ocorr[i]))
  oc_sq=$((oc_sq + ocorr[i] * ocorr[i]))
done
TMP_HTML+="$tfoot_u</tr>"$'\n'
TMP_HTML+="$tfoot_o</tr>"$'\n'

html_add <<'EOF'
      </tfoot>
    </table>
    <section id="estatisticas">
      <h2>Estatísticas</h2>
EOF

if (( n_total > 0 )); then
  m=$(( (soma_total * 200 + n_total) / (2 * n_total) ))
  printf -v media '%d.%02d' $((m / 100)) $((m % 100))
  if (( n_total > 1 )); then
    mr=$(( (rep_total * 200 + n_total - 1) / (2 * (n_total - 1)) ))
    printf -v rep_media '%d.%02d' $((mr / 100)) $((mr % 100))
  else
    rep_media='-'
  fi
  mo=$(( (oc_total * 200 + 31) / 62 ))
  printf -v oc_media '%d.%02d' $((mo / 100)) $((mo % 100))
  ma=$(( (at_total * 200 + at_cnt) / (2 * at_cnt) ))
  printf -v at_media '%d.%02d' $((ma / 100)) $((ma % 100))
  isqrt $((100000000 * (31 * oc_sq - oc_total * oc_total)))
  md=$(( (ISQRT + 1550) / 3100 ))
  printf -v oc_dp '%d.%02d' $((md / 100)) $((md % 100))
  oc_max=0
  oc_min=$n_total
  for ((i = 1; i <= 31; i++)); do
    if (( ocorr[i] > oc_max )); then oc_max=${ocorr[i]}; fi
    if (( ocorr[i] < oc_min )); then oc_min=${ocorr[i]}; fi
  done
  dz_max='' dz_min=''
  for ((i = 1; i <= 31; i++)); do
    if (( ocorr[i] == oc_max )); then dz_max+="${dz_max:+, }$i"; fi
    if (( ocorr[i] == oc_min )); then dz_min+="${dz_min:+, }$i"; fi
  done
  rot_max=dezena rot_min=dezena
  if [[ $dz_max == *,* ]]; then rot_max=dezenas; fi
  if [[ $dz_min == *,* ]]; then rot_min=dezenas; fi
  TMP_HTML+="      <p>Combinações = $n_total · Soma média = $media · Repetição média = $rep_media · "
  TMP_HTML+="<span class=\"dica\" title=\"7 dezenas × $n_total concursos ÷ 31 dezenas = 7 × $n_total ÷ 31. Não depende dos resultados, só do tamanho da amostra.\">Ocorrência média (sempre fixo nas $n_total amostras)</span> = $oc_media</p>"$'\n'
  TMP_HTML+="      <p><span class=\"dica\" title=\"Média da linha 'Último sorteio': há quantos concursos, em média, cada dezena não sai (dezenas que não saíram na amostra ficam de fora).\">Atraso médio</span> = $at_media · "
  TMP_HTML+="<span class=\"dica\" title=\"Desvio padrão da linha 'Ocorrência' entre as 31 dezenas. Quanto maior, mais desigual foi a frequência das dezenas na amostra.\">Desvio padrão da ocorrência</span> = $oc_dp · "
  TMP_HTML+="<span class=\"dica\" title=\"Maior valor da linha 'Ocorrência' e a(s) dezena(s) que o atingiram.\">Maior ocorrência</span> = $oc_max ($rot_max $dz_max) · "
  TMP_HTML+="<span class=\"dica\" title=\"Menor valor da linha 'Ocorrência' e a(s) dezena(s) que o atingiram.\">Menor ocorrência</span> = $oc_min ($rot_min $dz_min)</p>"$'\n'
  TMP_HTML+="      <table class=\"estat\">"$'\n'"        <tbody>"$'\n'
  if (( n_total > 1 )); then estat_linha Rep s_rep $((n_total - 1)); fi
  estat_linha Par s_par
  estat_linha Ímpar s_impar
  estat_linha Primo s_pri
  estat_linha SeqS s_seqs
  estat_linha SeqNS s_seqns
  estat_linha Mês s_mes "$n_total" mes
  TMP_HTML+="        </tbody>"$'\n'"      </table>"$'\n'
else
  TMP_HTML+="      <p>Nenhum concurso encontrado.</p>"$'\n'
fi

html_add <<'EOF'
    </section>
  </main>
</body>
</html>
EOF

printf '%s' "$TMP_HTML" > "$OUT"
if (( ! OFFLINE && mudou )); then
  printf '%s\n' "${todas[@]}" > "$TXT"
fi

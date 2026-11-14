#!/usr/bin/env bash
################################################################################
#   Consulta a API da Caixa e acrescenta ao .txt os concursos novos (mantem os
#   ultimos MAX_CONCURSOS, mais novo no fim). Regenera o HTML se houver
#   concurso novo ou se ele nao existir.
#
# Uso: ./lotofacil.sh [-o] [lf_results.txt] [index.html]
#
#   -o: modo offline: nao consulta a API e so regenera o HTML a partir do .txt.
################################################################################
set -euo pipefail

readonly URL_API='https://servicebus3.caixa.gov.br/portaldeloterias/api/lotofacil/'
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

TXT=${1:-lf_results.txt}
OUT=${2:-index.html}

if [[ -e $TXT && ! -r $TXT ]]; then
  erro "não foi possível ler '$TXT'"
fi

cores=(ff0000 ff4000 ff8000 ffbf00 ffff00 bfff00 80ff00 40ff00 00ff00 00ff40
       00ff80 00ffbf 00ffff 00bfff 0080ff 0040ff 0000ff 4000ff 8000ff bf00ff
       ff00ff ff00bf ff0080 ff0040 848484)

TMP_HTML=''

declare -A s_rep=() s_par=() s_impar=() s_pri=() s_l=() s_c=() s_seqs=() s_seqns=()
declare -a ant=() ocorr=() ult_pos=()
for ((i = 1; i <= 25; i++)); do
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
  LINHA=$(/usr/bin/jq -er --arg sep "$TXT_SEPARATOR" 'if (.numero | type) == "number"
       and (.dataApuracao | type) == "string"
       and (.listaDezenas | length) == 15
    then [.numero, .dataApuracao] + (.listaDezenas | sort_by(tonumber)) | join($sep)
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
  local n=${#ch[@]} a b t ct cb p pct

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
    p=$(( (ct * 20000 + total) / (2 * total) ))
    printf -v pct '%d.%02d' $((p / 100)) $((p % 100))
    TMP_HTML+="<td><b>$t</b> = $pct% ($ct)</td>"
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
  <title>Lotofácil - Últimos Resultados</title>
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
    h1 { margin: 8px 0; font-size: 1.2rem; }
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

    .resultados th:nth-child(n+3):nth-child(-n+27) { width: 2.2em; }

    .resultados th:nth-child(28),
    .resultados td:nth-child(28) { border-left: 2px solid #999; }

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
    .resultados tfoot td.vz { border: 0; }
    .resultados tfoot td.sm { border-left: 1px solid var(--borda); font-weight: 700; }
    .resultados tfoot td.mx { background: #ffb74d; font-weight: 700; }
    .resultados tfoot td.mn { background: #81d4fa; font-weight: 700; }
    .resultados tfoot td.nv { background: #e53935; color: #fff; font-weight: 700; }

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
    <img src="data:image/png;base64,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" alt="Lotofácil" loading="lazy">
    <h1>Últimos ${#todas[@]} resultados</h1>
    <small>Atualizado em <strong>${TMSTAMP}</strong></small>
  </header>
  <main>
    <table class="resultados">
      <thead>
        <tr>
          <th scope="col">Conc</th>
          <th scope="col">Data</th>
EOF

for ((i = 1; i <= 25; i++)); do
  TMP_HTML+="          <th scope=\"col\">${i}º</th>"$'\n'
done

html_add <<'EOF'
          <th scope="col" title="Dezenas repetidas do concurso anterior (o primeiro da amostra é sempre 0)">Rep</th>
          <th scope="col" title="Soma das dezenas">Som</th>
          <th scope="col" title="Quantidade de pares">Par</th>
          <th scope="col" title="Quantidade de ímpares">Imp</th>
          <th scope="col" title="Quantidade de primos">Pri</th>
          <th scope="col" title="Dezenas por linha do volante (ordem decrescente)">L</th>
          <th scope="col" title="Dezenas por coluna do volante (ordem decrescente)">C</th>
          <th scope="col" title="Maior sequência de dezenas sorteadas">SeqS</th>
          <th scope="col" title="Maior sequência de dezenas não sorteadas">SeqNS</th>
        </tr>
      </thead>
      <tbody>
EOF

for linha in "${todas[@]}"; do
  IFS="$TXT_SEPARATOR" read -r -a c <<< "$linha"
  (( ${#c[@]} >= 17 )) || continue

  row="        <tr><td>${c[0]}</td><td>${c[1]}</td>"
  k=2 soma=0 par=0 pri=0 rep=0
  seqs=0 seqns=0 run_s=0 run_ns=0
  lin=(0 0 0 0 0)
  col=(0 0 0 0 0)

  for ((i = 1; i <= 25; i++)); do
    if (( k <= 16 )) && (( i == 10#${c[k]} )); then
      row+="<td>$i</td>"
      (( k += 1 ))
      (( soma += i ))
      (( lin[(i - 1) / 5] += 1 ))
      (( col[(i - 1) % 5] += 1 ))
      if (( i % 2 == 0 )); then (( par += 1 )); fi
      case $i in 2|3|5|7|11|13|17|19|23) (( pri += 1 )) ;; esac
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

  L='' C=''
  for ((v = 5; v >= 0; v--)); do
    for ((j = 0; j < 5; j++)); do
      if (( lin[j] == v )); then L+=$v; fi
      if (( col[j] == v )); then C+=$v; fi
    done
  done
  impar=$((15 - par))

  TMP_HTML+="$row<td>$rep</td><td>$soma</td><td>$par</td><td>$impar</td><td>$pri</td><td>$L</td><td>$C</td><td>$seqs</td><td>$seqns</td></tr>"$'\n'

  (( n_total += 1 ))
  (( soma_total += soma ))
  if (( n_total > 1 )); then
    rep_total=$((rep_total + rep))
    (( s_rep[$rep] += 1 ))
  fi
  (( s_par[$par] += 1 ))
  (( s_impar[$impar] += 1 ))
  (( s_pri[$pri] += 1 ))
  (( s_l[$L] += 1 ))
  (( s_c[$C] += 1 ))
  (( s_seqs[$seqs] += 1 ))
  (( s_seqns[$seqns] += 1 ))
done

html_add <<'EOF'
      </tbody>
      <tfoot>
EOF

ult_v=()
um_max=-1
um_min=$n_total
oc_max=0
oc_min=$n_total
for ((i = 1; i <= 25; i++)); do
  ult_v[i]=-1
  if (( ult_pos[i] > 0 )); then
    ult_v[i]=$((n_total - ult_pos[i]))
    if (( ult_v[i] > um_max )); then um_max=${ult_v[i]}; fi
    if (( ult_v[i] < um_min )); then um_min=${ult_v[i]}; fi
    at_total=$((at_total + ult_v[i]))
    at_cnt=$((at_cnt + 1))
  fi
  if (( ocorr[i] > oc_max )); then oc_max=${ocorr[i]}; fi
  if (( ocorr[i] < oc_min )); then oc_min=${ocorr[i]}; fi
  oc_total=$((oc_total + ocorr[i]))
  oc_sq=$((oc_sq + ocorr[i] * ocorr[i]))
done

tfoot_u="        <tr><th colspan=\"2\" scope=\"row\" title=\"Concursos desde que a dezena foi sorteada (0 = saiu no último). Laranja = maior, azul = menor\">Último sorteio</th>"
tfoot_o="        <tr><th colspan=\"2\" scope=\"row\" title=\"Vezes que a dezena foi sorteada na amostra. Laranja = maior, azul = menor\">Ocorrência</th>"
for ((i = 1; i <= 25; i++)); do
  cls_u='' cls_o=''
  if (( oc_max != oc_min )); then
    if (( ocorr[i] == oc_max )); then
      cls_o=' class="mx"'
    elif (( ocorr[i] == oc_min )); then
      cls_o=' class="mn"'
    fi
  fi
  if (( ult_v[i] >= 0 )); then
    if (( um_max != um_min )); then
      if (( ult_v[i] == um_max )); then
        cls_u=' class="mx"'
      elif (( ult_v[i] == um_min )); then
        cls_u=' class="mn"'
      fi
    fi
    tfoot_u+="<td$cls_u>${ult_v[i]}</td>"
  else
    tfoot_u+="<td class=\"nv\">-</td>"
  fi
  tfoot_o+="<td$cls_o>${ocorr[i]}</td>"
done
if (( n_total > 0 )); then
  soma_media=$(( (soma_total * 2 + n_total) / (2 * n_total) ))
  tfoot_u+="<td class=\"vz\"></td><td class=\"sm\" title=\"Soma média dos $n_total concursos\">$soma_media</td>"
fi
TMP_HTML+="$tfoot_u</tr>"$'\n'
TMP_HTML+="$tfoot_o</tr>"$'\n'

html_add <<'EOF'
      </tfoot>
    </table>
    <section id="estatisticas">
      <h2>Estatísticas</h2>
EOF

if (( n_total > 0 )); then
  if (( n_total > 1 )); then
    mr=$(( (rep_total * 200 + n_total - 1) / (2 * (n_total - 1)) ))
    printf -v rep_media '%d.%02d' $((mr / 100)) $((mr % 100))
  else
    rep_media='-'
  fi
  mo=$(( (oc_total * 200 + 25) / 50 ))
  printf -v oc_media '%d.%02d' $((mo / 100)) $((mo % 100))
  ma=$(( (at_total * 200 + at_cnt) / (2 * at_cnt) ))
  printf -v at_media '%d.%02d' $((ma / 100)) $((ma % 100))
  isqrt $((160000 * (25 * oc_sq - oc_total * oc_total)))
  md=$(( (ISQRT + 50) / 100 ))
  printf -v oc_dp '%d.%02d' $((md / 100)) $((md % 100))
  dz_max='' dz_min=''
  for ((i = 1; i <= 25; i++)); do
    if (( ocorr[i] == oc_max )); then dz_max+="${dz_max:+, }$i"; fi
    if (( ocorr[i] == oc_min )); then dz_min+="${dz_min:+, }$i"; fi
  done
  rot_max=dezena rot_min=dezena
  if [[ $dz_max == *,* ]]; then rot_max=dezenas; fi
  if [[ $dz_min == *,* ]]; then rot_min=dezenas; fi
  TMP_HTML+="      <p>Repetição média = $rep_media · "
  TMP_HTML+="<span class=\"dica\" title=\"15 dezenas × $n_total concursos ÷ 25 dezenas = 0,6 × $n_total. Não depende dos resultados, só do tamanho da amostra.\">Ocorrência média (sempre fixo nas $n_total amostras)</span> = $oc_media</p>"$'\n'
  TMP_HTML+="      <p><span class=\"dica\" title=\"Média da linha 'Último sorteio': há quantos concursos, em média, cada dezena não sai (dezenas que não saíram na amostra ficam de fora).\">Atraso médio</span> = $at_media · "
  TMP_HTML+="<span class=\"dica\" title=\"Desvio padrão da linha 'Ocorrência' entre as 25 dezenas. Quanto maior, mais desigual foi a frequência das dezenas na amostra.\">Desvio padrão da ocorrência</span> = $oc_dp · "
  TMP_HTML+="<span class=\"dica\" title=\"Maior valor da linha 'Ocorrência' e a(s) dezena(s) que o atingiram.\">Maior ocorrência</span> = $oc_max ($rot_max $dz_max) · "
  TMP_HTML+="<span class=\"dica\" title=\"Menor valor da linha 'Ocorrência' e a(s) dezena(s) que o atingiram.\">Menor ocorrência</span> = $oc_min ($rot_min $dz_min)</p>"$'\n'
  TMP_HTML+="      <table class=\"estat\">"$'\n'"        <tbody>"$'\n'
  if (( n_total > 1 )); then estat_linha Rep s_rep $((n_total - 1)); fi
  estat_linha Par s_par
  estat_linha Ímpar s_impar
  estat_linha Primo s_pri
  estat_linha L s_l
  estat_linha C s_c
  estat_linha SeqS s_seqs
  estat_linha SeqNS s_seqns
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

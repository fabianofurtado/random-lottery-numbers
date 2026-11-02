/*
 * 2018-05-05 by Fabiano Furtado
 * 
 * Based on: http://www.learn-c.org/en/Linked_lists
 * 
 * * * * * * * * * * * * * * * * * * * * * * * 
 * Generate the random list number:
 *   https://www.random.org/integer-sets/?sets=10&num=15&min=1&max=25&sort=on&order=values&format=plain&rnd=new
 * 
 * * * * * * * * * * * * * * * * * * * * * * * 
 * Compilar com:
 *   gcc -Wall -Wextra -O2 lf_filter.c -o lf_filter -lconfig
 *
 * * * * * * * * * * * * * * * * * * * * * * * 
 * Formato do arquivo lf_filter.conf (exemplo):
 * ultimo_concurso = {
 *   numero  = 1780;
 *   dezenas = [3,4,5,6,7,9,10,11,13,14,15,16,17,22,23];
 * };
 *
 * * * * * * * * * * * * * * * * * * * * * * *
 * Historico:
 *   * 2026-11-02 - Inclusao dos parametros "-m" (maximo de primos) e "-v"
 *                  (estatisticas de cada combinacao)
 *                - Estatisticas calculadas uma unica vez por combinacao
 *                  (stats_T) e usadas pelos filtros e pela impressao
 *   * 2026-11-01 - Correcao no RNG
 *   * 2019-06-04 - Correcao de bugs
 *                - Inclusao do parametro "a11"
 *   * 2019-05-15 - Uso de variavel static para parametros
 *                - Inclusao da SOMA na saida
 *                - Saida com combinacoes unicas quando se utiliza
 *                  numeros randomicos
 *   * 2019-05-14 - Geracao de numeros randomicos
 *   * 2019-02-25 - Add "-r" option
 */
#define _DEFAULT_SOURCE  /* arc4random_uniform com -std=c99 ou -std=c11 */
#include <ctype.h>
#include <errno.h>
#include <libconfig.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NOM                   "Lotofacil"
#define NUM_MINIMO            1
#define NUM_MAXIMO            25
#define QTD_NUM_SORTEADOS     15
#define QTD_NUM_NAO_SORTEADOS ( NUM_MAXIMO - QTD_NUM_SORTEADOS )
#define QTD_LINHAS            5
#define QTD_COLUNAS           5
#define NUM_MAX_FIXOS         ( QTD_NUM_SORTEADOS - 1 )
#define NUM_MAX_DESCARTADOS   ( QTD_NUM_NAO_SORTEADOS - 1 )
#define SOMA_MIN              ( 1+2+3+4+5+6+7+8+9+10+11+12+13+14+15 )
#define SOMA_MAX              ( 11+12+13+14+15+16+17+18+19+20+21+22+23+24+25 )
#define MAX_LINE              64
#define MAX_COMB              3268760UL // C(25,15) = C(25,10) =
                                        //                    25!/(15!*(25-15)!)
#define CONF_FILE             "lf_filter.conf"
#define LF_MAX_PRIME          9
#define UNDEFINED_VALUE       -1


typedef struct {
  int numero; /* por questoes de compatibilidade com a libconfig, estou usando
                 int mas gostaria de estar usando unsigned short int */
  uint8_t a_dezenas[QTD_NUM_SORTEADOS];
} concurso_T;

typedef struct {
  uint32_t mask;      /* bit n ligado = numero n sorteado (n = 1..25) */
  uint16_t soma,
           linha,     /* padrao das linhas, ex: 43332  */
           coluna;    /* padrao das colunas, ex: 44322 */
  uint8_t  pares,
           primos,
           repetidos, /* repetidos do concurso anterior (so com -a) */
           seq_sort,  /* maior sequencia de numeros sorteados     */
           seq_desc;  /* maior sequencia de numeros NAO sorteados */
} stats_T;

typedef struct node {
  uint8_t a_lt[QTD_NUM_SORTEADOS]; /* array of lottery ticket */
  stats_T st;         /* calculado por calc_stats() */
  struct node * next;
} node_T;

typedef struct {
  bool verbose,
       p6, p7, p8, p9,
       a7, a8, a9, a10, a11,
       dl43332, dl44322, dl44331, dl53322, dl54321,
       dc43332, dc44322, dc44331, dc53322, dc54321;
  concurso_T *ultimo_concurso; /* algum "-a" informado: le o arquivo .conf */
  uint8_t qtd_fixos,
          qtd_descartados,
          seq_min_sorteados,
          seq_min_descartados;
  int8_t  primo_min,
          primo_max;
  uint16_t soma_min, // 1+2+3+4+5+6+7+8+9+10+11+12+13+14+15          = 120 min
           soma_max; // 11+12+13+14+15+16+17+18+19+20+21+22+23+24+25 = 270 max
  uint32_t mask_fixos,       /* bit n ligado = numero n fixo       */
           mask_descartados; /* bit n ligado = numero n descartado */
  uint64_t qtd_comb_randomicas;
} param_T;


enum {
  LINE_OK = 0,
  LINE_FORMAT,
  LINE_RANGE,
  LINE_DUP
};


static int
qsort_asc( const void * a, const void * b )
{
   return ( *(uint8_t*)a - *(uint8_t*)b );
}

static int
qsort_desc( const void * a, const void * b )
{
   return ( *(uint8_t*)b - *(uint8_t*)a );
}


static bool
is_lf_prime( uint8_t n )
{
  uint8_t uc;
  const uint8_t lf_prime[LF_MAX_PRIME] = { 2, 3, 5, 7, 11, 13, 17, 19, 23 };


  for ( uc = 0 ; uc < LF_MAX_PRIME ; uc++ ) {
    if ( n  == lf_prime[uc] )
      return true;
  }
  return false;
}


static void
get_valid_sequence( node_T *node ) // Baseado em Fisher-Yates
{
  uint32_t i, /* contador do laco e fronteira do baralho. Marca quantas cartas
                 ja foram sorteadas (0..i-1) e onde comeca a regiao ainda
                 disponivel (i..NUM_MAXIMO-1). Tambem e o indice onde o numero
                 sorteado vai parar, tanto em tmp_pool quanto em a_lt. */
           j; /* indice sorteado. Aponta, dentro da região disponivel
                 (i..NUM_MAXIMO-1), qual carta foi escolhida nesta iteracao.
                 Vale i + um deslocamento aleatorio de 0 a NUM_MAXIMO-1-i. */
  uint8_t  tmp,
           tmp_pool[NUM_MAXIMO];


  for( i = 0 ; i < NUM_MAXIMO ; i++ )
    tmp_pool[i] = i + NUM_MINIMO;
  for( i = 0 ; i < QTD_NUM_SORTEADOS ; i++ ) {
    j = i + arc4random_uniform( NUM_MAXIMO - i ); // glibc >= 2.36
    tmp = tmp_pool[i];
    tmp_pool[i] = tmp_pool[j];
    tmp_pool[j] = tmp;
    node->a_lt[i] = tmp_pool[i];
  }
}


static uint8_t
conta_pares( const uint8_t *lottery_ticket )
{
  uint8_t i,
          n = 0;


  for ( i = 0 ; i < QTD_NUM_SORTEADOS ; i++ )
    if ( lottery_ticket[i] % 2 == 0 )
      n++;

  return n;
}


static uint8_t
conta_primos( const uint8_t *lottery_ticket )
{
  uint8_t i, n = 0;


  for ( i = 0 ; i < QTD_NUM_SORTEADOS ; i++ )
    if ( is_lf_prime( lottery_ticket[i] ) )
      n++;

  return n;
}


static uint8_t
conta_repetidos_uc( const uint8_t *lt, const concurso_T *uc )
{
  uint8_t i,
          n = 0;

  if ( uc != NULL ) {
    for ( i = 0 ; i < QTD_NUM_SORTEADOS ; i++ )
      if ( memchr( uc->a_dezenas, lt[i], QTD_NUM_SORTEADOS ) != NULL )
        n++;
  }

  return n;
}


static uint16_t
padrao_linha( const uint8_t *lottery_ticket )
{
  uint8_t  i,
           linha[QTD_LINHAS] = { 0 };
  uint16_t padrao = 0;


  for ( i = 0 ; i < QTD_NUM_SORTEADOS ; i++ )
    linha[ ( lottery_ticket[i] - 1 ) / 5 ]++;
  qsort( linha, QTD_LINHAS, sizeof( linha[0] ), qsort_desc );
  for ( i = 0 ; i < QTD_LINHAS ; i++ )
    padrao = 10 * padrao + linha[i];

  return padrao;
}

/* A Lotofacil tem 5 colunas: (n-1) % 5
 * agrupa 1|6|11|16|21, 2|7|12|17|22, 3|8|13|18|23, 4|9|14|19|24, 5|10|15|20|25
 * Como as contagens sao ordenadas depois, a ordem das colunas nao importa.
 */
static uint16_t
padrao_coluna( const uint8_t *lottery_ticket )
{
  uint8_t  i,
           coluna[QTD_COLUNAS] = { 0 };
  uint16_t padrao = 0;


  for ( i = 0 ; i < QTD_NUM_SORTEADOS ; i++ )
    coluna[ ( lottery_ticket[i] - 1 ) % 5 ]++;
  qsort( coluna, QTD_COLUNAS, sizeof( coluna[0] ), qsort_desc );
  for ( i = 0 ; i < QTD_COLUNAS ; i++ )
    padrao = 10 * padrao + coluna[i];

  return padrao;
}


/* lottery_ticket precisa estar ordenado */
static uint8_t
max_seq_sorteados( const uint8_t *lottery_ticket )
{
  uint8_t i,
          seq = 1,
          max = 1;


  for ( i = 1 ; i < QTD_NUM_SORTEADOS ; i++ ) {
    if ( lottery_ticket[i] == lottery_ticket[i-1] + 1 ) {
      if ( ++seq > max )
        max = seq;
    } else
      seq = 1;
  }

  return max;
}


static uint8_t
max_seq_descartados( const uint8_t *lottery_ticket )
{
  bool    drawn[NUM_MAXIMO + 1] = { false };
  uint8_t i,
          seq = 0,
          max = 0;


  for ( i = 0 ; i < QTD_NUM_SORTEADOS ; i++ )
    drawn[lottery_ticket[i]] = true;
  for ( i = NUM_MINIMO ; i <= NUM_MAXIMO ; i++ ) {
    if ( drawn[i] )
      seq = 0;
    else if ( ++seq > max )
      max = seq;
  }

  return max;
}


static void
calc_stats( node_T *node, const concurso_T *uc )
{
  stats_T       *s = &node->st;
  const uint8_t *lt = node->a_lt;  /* precisa estar ordenado */
  uint8_t        i;

  s->soma = 0;
  s->mask = 0;
  for ( i = 0 ; i < QTD_NUM_SORTEADOS ; i++ ) {
    s->soma += lt[i];
    s->mask |= UINT32_C(1) << lt[i];
  }
  s->pares     = conta_pares( lt );
  s->primos    = conta_primos( lt );
  s->repetidos = conta_repetidos_uc( lt, uc );
  s->linha     = padrao_linha( lt );
  s->coluna    = padrao_coluna( lt );
  s->seq_sort  = max_seq_sorteados( lt );
  s->seq_desc  = max_seq_descartados( lt );
}


/* true se a combinacao passa em todos os filtros (usa apenas node->st) */
static bool
ticket_ok( const node_T *node, const param_T *p )
{
  const stats_T *s = &node->st;

  if ( ( p->p6 || p->p7 || p->p8 || p->p9 ) &&
       !( ( p->p6 && s->pares == 6 ) || ( p->p7 && s->pares == 7 ) ||
          ( p->p8 && s->pares == 8 ) || ( p->p9 && s->pares == 9 ) ) )
    return false;

  if ( p->ultimo_concurso != NULL &&
       ( ( p->a7  && s->repetidos ==  7 ) || ( p->a8  && s->repetidos ==  8 ) ||
         ( p->a9  && s->repetidos ==  9 ) || ( p->a10 && s->repetidos == 10 ) ||
         ( p->a11 && s->repetidos == 11 ) ) == false )
    return false;

  if ( p->soma_min > 0 && s->soma < p->soma_min )
    return false;
  if ( p->soma_max > 0 && s->soma > p->soma_max )
    return false;

  /* todos os fixos precisam estar na combinacao */
  if ( ( s->mask & p->mask_fixos ) != p->mask_fixos )
    return false;
  /* nenhum descartado pode estar na combinacao */
  if ( ( s->mask & p->mask_descartados ) != 0 )
    return false;

  if ( p->seq_min_sorteados > 0 && s->seq_sort < p->seq_min_sorteados )
    return false;
  if ( p->seq_min_descartados > 0 && s->seq_desc < p->seq_min_descartados )
    return false;

  if ( p->primo_min != UNDEFINED_VALUE && s->primos < p->primo_min )
    return false;
  if ( p->primo_max != UNDEFINED_VALUE && s->primos > p->primo_max )
    return false;

  if ( ( p->dl43332 || p->dl44322 || p->dl44331 || p->dl53322 || p->dl54321 ) &&
       !( ( p->dl43332 && s->linha == 43332 ) ||
          ( p->dl44322 && s->linha == 44322 ) ||
          ( p->dl44331 && s->linha == 44331 ) ||
          ( p->dl53322 && s->linha == 53322 ) ||
          ( p->dl54321 && s->linha == 54321 ) ) )
    return false;

  if ( ( p->dc43332 || p->dc44322 || p->dc44331 || p->dc53322 || p->dc54321 ) &&
       !( ( p->dc43332 && s->coluna == 43332 ) ||
          ( p->dc44322 && s->coluna == 44322 ) ||
          ( p->dc44331 && s->coluna == 44331 ) ||
          ( p->dc53322 && s->coluna == 53322 ) ||
          ( p->dc54321 && s->coluna == 54321 ) ) )
    return false;

  return true;
}


static void
print_node( const node_T *node, const param_T *p )
{
  uint8_t        i;
  const stats_T *s = &node->st;

  for ( i = 0 ; i < QTD_NUM_SORTEADOS ; i++ )
    printf( i > 0 ? " %02d" : "%02d", node->a_lt[i] );

  if ( p->verbose == true ) {
    printf( "   Soma=%d  Pares=%02d  Impares=%02d  Primos=%d",
            s->soma, s->pares, QTD_NUM_SORTEADOS - s->pares, s->primos );
    if ( p->ultimo_concurso != NULL )
      printf( "  Rep=%d", s->repetidos );
    printf( "  L=%d  C=%d  SeqS=%d  SeqNS=%d",
            s->linha, s->coluna, s->seq_sort, s->seq_desc );
  }
  printf( "\n" );
}


static void
print_list( const node_T *head, const param_T *p )
{
  const node_T *current = head;

  while ( current != NULL ) {
    print_node( current, p );
    current = current->next;
  }
}


static void
pop_head_node( node_T **head )
{
  node_T * next_node = NULL;

  if (*head == NULL) {
     return;
  }

  next_node = (*head)->next;
  free(*head);
  *head = next_node;
}


/*
static void
pop_by_index( node_T **head, uint8_t index )
{
    uint8_t i = 0;
    node_T * current = *head;
    node_T * temp_node = NULL;

    if ( index == 0 ) {
      pop_head_node(head);
      return;
    }

    for ( i = 0; i < index-1; i++ ) {
      // return without deleting de node - index out of range
      if ( current->next == NULL ) {
        return;
      }
      current = current->next;
    }

    temp_node = current->next;
    current->next = temp_node->next;
    free( temp_node );
}
*/

static void
free_list( node_T **head )
{
  while ( *head != NULL ) {
    pop_head_node(head);
  }
}


static node_T*
create_node( void )
{
  node_T * new_node = NULL;
  new_node = calloc( 1, sizeof( *new_node ) );
  if (new_node == NULL) {
    perror("create_node(): cannot alocate memory!");
    exit(1);
  }    
  return new_node;
}

/*
static node_T*
create_clean_node()
{
  node_T *new_node = create_node();

  memset( new_node, 0, sizeof( *new_node ) );

  return new_node;
}
*/

/* add an item to the end of the list */
static bool
push_node_end( node_T **head, node_T *tmp_node, const bool unique_combination )
{
  node_T *current = *head;
  node_T *last    = NULL;

  /* percorre toda a lista; compara inclusive o ultimo no */
  while ( current != NULL ) {
    if ( unique_combination &&
         memcmp( current->a_lt, tmp_node->a_lt, sizeof( current->a_lt ) ) == 0 )
      return false;               /* duplicata: nao insere */
    last    = current;
    current = current->next;
  }

  node_T *new_node = create_node();   /* calloc: next ja vem NULL */
  memcpy( new_node->a_lt, tmp_node->a_lt, sizeof( new_node->a_lt ) );
  new_node->st = tmp_node->st;

  if ( last == NULL )
    *head = new_node;             /* lista vazia */
  else
    last->next = new_node;

  return true;
}


static void
read_conf_file( concurso_T *ultimo_concurso )
{
  uint8_t           c;
  int               num;
  bool              seen[NUM_MAXIMO + 1] = { false }; // index 0 ate 25
  config_t          cfg;
  config_setting_t *setting,
                   *dezenas;
  const char       *ef;

  config_init( &cfg );

  if ( config_read_file( &cfg, CONF_FILE ) == CONFIG_FALSE ) {
    ef = config_error_file( &cfg );
    fprintf( stderr,
             "Erro ao abrir o arquivo \"%s\"!\nDetalhes: %s:%d - %s\n",
             CONF_FILE,
             ef ? ef : CONF_FILE,
             config_error_line( &cfg ),
             config_error_text( &cfg ) );
    config_destroy( &cfg );
    exit( EXIT_FAILURE );
  }

  if ( ( setting = config_lookup( &cfg, "ultimo_concurso" ) ) == NULL ) {
    fprintf( stderr, "Erro! Entrada \"ultimo_concurso\" nao encontrada!\n\n" );
    config_destroy( &cfg );
    exit( EXIT_FAILURE );
  }

  if ( config_setting_lookup_int( setting, "numero",
                                  &(ultimo_concurso->numero) ) == CONFIG_FALSE){
    fprintf( stderr, "ERRO! \"numero\" precisa ser definido!\n\n" );
    config_destroy( &cfg );
    exit( EXIT_FAILURE );
  }

  if ( ( dezenas = config_setting_get_member( setting, "dezenas" ) ) == NULL ) {
    fprintf( stderr, "ERRO! \"dezenas\" precisa ser definido!\n\n" );
    config_destroy( &cfg );
    exit( EXIT_FAILURE );
  }

  if ( config_setting_length( dezenas ) != QTD_NUM_SORTEADOS ) {
    fprintf( stderr, "ERRO! %d dezenas precisam ser definidas!\n",
             QTD_NUM_SORTEADOS );
    config_destroy( &cfg );
    exit( EXIT_FAILURE );
  }

  for ( c = 0 ; c < QTD_NUM_SORTEADOS ; c++ ) {
    num = config_setting_get_int_elem( dezenas, c );
    if ( num < NUM_MINIMO || num > NUM_MAXIMO ) {
      fprintf( stderr,
               "ERRO! A dezena de posicao %d precisa estar entre %d e %d!\n",
               c + 1, NUM_MINIMO, NUM_MAXIMO );
      config_destroy( &cfg );
      exit( EXIT_FAILURE );
    }
    if ( seen[num] ) {
      fprintf( stderr, "ERRO! A dezena %d esta repetida no arquivo %s!\n",
               num, CONF_FILE );
      config_destroy( &cfg );
      exit( EXIT_FAILURE );
    }
    seen[num] = true;
    ultimo_concurso->a_dezenas[c] = (uint8_t)num;
  }

  config_destroy( &cfg );
}


static int
parse_line( char *line, node_T *node )
{
  char  *tok, *save, *end;
  size_t cnt = 0;
  long   v;
  bool   seen[NUM_MAXIMO + 1] = { false };

  for ( tok = strtok_r( line, " \t\r\n", &save ) ;
        tok != NULL ;
        tok = strtok_r( NULL, " \t\r\n", &save ) ) {
    if ( cnt >= QTD_NUM_SORTEADOS )
      return LINE_FORMAT;
    if ( isdigit( (unsigned char)*tok ) == 0 )   /* rejeita +1, -1, etc */
      return LINE_FORMAT;
    errno = 0;
    v = strtol( tok, &end, 10 );
    if ( errno == ERANGE || ( errno == 0 && *end == '\0' &&
         ( v < NUM_MINIMO || v > NUM_MAXIMO ) ) )
      return LINE_RANGE;
    if ( errno != 0 || *end != '\0' )
      return LINE_FORMAT;
    if ( seen[v] )
      return LINE_DUP;
    seen[v] = true;
    node->a_lt[cnt++] = (uint8_t)v;
  }
  return cnt == QTD_NUM_SORTEADOS ? LINE_OK : LINE_FORMAT;
}


static bool
parse_ulong( const char *s, unsigned long min, unsigned long max,
             unsigned long *out )
{
  char *end;
  unsigned long v;


  if ( s == NULL || isdigit( (unsigned char)*s ) == 0 )
    return false;
  errno = 0;
  v = strtoul( s, &end, 10 );
  if ( errno != 0 || *end != '\0' || v < min || v > max )
    return false;
  *out = v;
  return true;
}


static uint64_t
binomial( uint64_t n, uint64_t k )
{
  uint64_t r = 1,
           i;

  if ( k > n )
    return 0;
  if ( k > n - k )
    k = n - k;
  for ( i = 1 ; i <= k ; i++ )
    r = r * ( n - k + i ) / i;   /* divisao sempre exata a cada passo */
  return r;
}


static void
print_help( const char *argv0 )
{
  fprintf( stderr,
           "\n[ERRO] Gerador de numeros da " NOM
           ": Numero de parametros invalidos!\n\n"
           "Uso: %s [FILTRO] [-v] -n[numero combinacoes randomicas]|[arq matriz]\n\n"
           "  -v, mostra as estatisticas de cada combinacao (soma, pares, primos, ...)\n\n"
           "Onde [FILTRO]:\n"
           "  -p <quantidade de pares>\n"
           "     Exemplo: -p7, filtra por 7 pares (e 8 impares)\n"
           "              -p8, filtra por 8 pares (e 7 impares)\n"
           "              -p6, filtra por 6 pares (e 9 impares)\n"
           "              -p9, filtra por 9 pares (e 6 impares)\n"
           "  -a<quantidade de numeros repetidos do concurso anterior>\n"
           "     Estatisticamente:\n"
           "       -a9  = 33.44%%\n"
           "       -a8  = 25.21%%\n"
           "       -a10 = 21.14%%\n"
           "       -a7  =  9.12%%\n"
           "       -a11 =  8.13%%\n"
           "  -i<min>, soma MINIMA de todos os numeros (%d a %d)\n"
           "  -x<max>, soma MAXIMA de todos os numeros (%d a %d)\n"
           "    * Estatisticamente, recomanda-se <min> = 184 e <max> = 199\n"
           "      O valor medio e 195.\n"
           "  -f<numero fixo>, fixa um numero na combinacao\n"
           "    * Obs: o numero maximo de fixos e %d.\n"
           "  -d<numero descartado>, descarta um numero na combinacao\n"
           "    * Obs: o numero maximo de descartados e %d.\n"
           "  -l<distribuicao>, Distribuicao dos Numeros nas Linhas em ordem decrescente\n"
           "    Possiveis valores para <distribuicao>:\n"
           "      * 43332\n"
           "      * 44322\n"
           "      * 44331\n"
           "      * 53322\n"
           "      * 54321\n"
           "  -c<distribuicao>, Distribuicao dos Numeros nas Colunas em ordem decrescente\n"
           "    Possiveis valores para <distribuicao>:\n"
           "      * 43332\n"
           "      * 44322\n"
           "      * 44331\n"
           "      * 53322\n"
           "      * 54321\n"
           "  -s<seq minimo S> (2 a %d), filtra por uma sequencia minima de numeros sorteados.\n"
           "    * Exemplo: a combinacao ...02 04 06 07 08 09 13 14...\n"
           "      possui uma sequencia minima de 4 numeros sorteados  (06 07 08 e 09).\n"
           "      Neste caso, o parametro para filtro seria '-s4'.\n"
           "      Um bom parametro para o filtro seria '-s4' ou '-s5'.\n"
           "  -e<seq minimo NS> (2 a %d), filtra por uma sequencia minima de numeros NAO sorteados.\n"
           "    * Exemplo: a combinacao ...02 04 06 07 08 09 13 14...\n"
           "      possui uma sequencia minima de 3 numeros nao sorteados (10 11 e 12).\n"
           "      Neste caso, o parametro para filtro seria '-e3'.\n"
           "      Um bom parametro para o filtro seria '-e3' ou '-e4'.\n"
           "  -r<qtd MINIMA primos>, filtra por uma quantidade MINIMA de numeros primos no sorteio.\n"
           "    * Obs: Valores permitidos: -r1 ate -r%d\n"
           "      Possiveis numeros primos para LF: 2, 3, 5, 7, 11, 13, 17, 19, 23\n"
           "      Estatisticamente, recomanda-se, em media, o uso de '-r5'\n"
           "  -m<qtd MAXIMA primos>, filtra por uma quantidade MAXIMA de numeros primos no sorteio.\n"
           "    * Obs: Valores permitidos: -m0 ate -m%d (-m0 -nenhum primo- é possível!)\n\n",
           argv0,
           SOMA_MIN, SOMA_MAX,         /* -i */
           SOMA_MIN, SOMA_MAX,         /* -x */
           NUM_MAX_FIXOS,              /* -f */
           NUM_MAX_DESCARTADOS,        /* -d */
           QTD_NUM_SORTEADOS,          /* -s */
           QTD_NUM_NAO_SORTEADOS,      /* -e */
           LF_MAX_PRIME,               /* -r */
           LF_MAX_PRIME );             /* -m */
}


int
main( int argc, char **argv )
{
  static concurso_T ultimo_concurso;
  static param_T  p = { .primo_min = UNDEFINED_VALUE,
                        .primo_max = UNDEFINED_VALUE };
  static node_T  *head = NULL, /* Declare the head of the list.
                                  The head is used as a reference for all
                                  functions. */
                 *tmp_node = NULL;
  FILE           *fp;
  char           str[ MAX_LINE ];
  int            rc;  // return code
  uint64_t       usi = 0,
                 total_possivel;
  unsigned long  ul;
  int            caractere;


  if ( argc < 2 ) {
    print_help( argv[0] );
    return 1;
  }

  //memset( &p, 0, sizeof( *p ) ); // retirado. p eh static
  opterr = 0;

  while ( ( caractere = getopt( argc, argv, "vn:p:a:i:x:f:d:l:c:s:e:r:m:" ) ) !=
                                                               UNDEFINED_VALUE )
  {
    switch ( caractere ) {
      case 'v': p.verbose = true;
        break;
      case 'n':
        if ( parse_ulong( optarg, 1, MAX_COMB, &ul ) == false ) {
          fprintf( stderr,
                   "\nERRO! Parametro de combinacoes randomicas '-n%s' "
                   "invalido!\n\n",
                   optarg );
          return 1;
        }
        p.qtd_comb_randomicas = (uint64_t)ul;
        break;
      case 'p':
        if ( parse_ulong( optarg, 6, 9, &ul ) == false ) {
          fprintf( stderr,
                   "\nERRO! Parametro de pares e impares '-p%s' invalido!\n\n",
                   optarg );
          return 1;
        }
        switch ( ul ) {
          case 6: p.p6 = true; break;
          case 7: p.p7 = true; break;
          case 8: p.p8 = true; break;
          case 9: p.p9 = true; break;
        }
        break;
      case 'a':
        if ( parse_ulong( optarg, 7, 11, &ul ) == false ) {
          fprintf( stderr,
                   "\nERRO! Parametro \"numeros repetidos do concurso "
                   "anterior\" '-a%s' invalido!\n\n",
                   optarg );
          return 1;
        }
        switch ( ul ) {
          case  7: p.a7  = true; break;
          case  8: p.a8  = true; break;
          case  9: p.a9  = true; break;
          case 10: p.a10 = true; break;
          case 11: p.a11 = true; break;
        }
        if ( p.ultimo_concurso == NULL ) {   /* le o .conf apenas uma vez */
          read_conf_file( &ultimo_concurso );
          p.ultimo_concurso = &ultimo_concurso;
        }
        break;
      case 'i':
        if ( parse_ulong( optarg, SOMA_MIN, SOMA_MAX, &ul ) == false ) {
          fprintf( stderr,
                   "\nERRO! Parametro de soma minima '-i%s' invalido!\n\n",
                   optarg );
          return 1;
        }
        p.soma_min = (uint16_t)ul;
        break;
      case 'x':
        if ( parse_ulong( optarg, SOMA_MIN, SOMA_MAX, &ul ) == false ) {
          fprintf( stderr,
                   "\nERRO! Parametro de soma maxima '-x%s' invalido!\n\n",
                   optarg );
          return 1;
        }
        p.soma_max = (uint16_t)ul;
        break;
      case 'f':
        if ( parse_ulong( optarg, NUM_MINIMO, NUM_MAXIMO, &ul ) == false ) {
          fprintf( stderr,
                   "\nERRO! Parametro '-f%s' invalido! "
                   "Deve estar entre %d e %d.\n\n",
                   optarg, NUM_MINIMO, NUM_MAXIMO );
          return 1;
        }
        if ( ( p.mask_fixos & ( UINT32_C(1) << ul ) ) != 0 ) {
          fprintf( stderr, "\nERRO! Numero fixo %lu repetido em -f!\n\n", ul );
          return 1;
        }
        if ( p.qtd_fixos + 1 > NUM_MAX_FIXOS ) {
          fprintf( stderr,
                   "\nERRO! Nao podem existir mais que %d numeros fixos!\n\n",
                   NUM_MAX_FIXOS );
          return 1;
        }
        p.mask_fixos |= UINT32_C(1) << ul;
        p.qtd_fixos++;
        break;
      case 'd':
        if ( parse_ulong( optarg, NUM_MINIMO, NUM_MAXIMO, &ul ) == false ) {
          fprintf( stderr,
                   "\nERRO! Parametro '-d%s' invalido! "
                   "Deve estar entre %d e %d.\n\n",
                   optarg, NUM_MINIMO, NUM_MAXIMO );
          return 1;
        }
        if ( ( p.mask_descartados & ( UINT32_C(1) << ul ) ) != 0 ) {
          fprintf( stderr,
                   "\nERRO! Numero descartado %lu repetido em -d!\n\n",
                   ul );
          return 1;
        }
        if ( p.qtd_descartados + 1 > NUM_MAX_DESCARTADOS ) {
          fprintf( stderr,
                   "\nERRO! Nao podem existir mais que %d numeros "
                   "descartados!\n\n",
                   NUM_MAX_DESCARTADOS );
          return 1;
        }
        p.mask_descartados |= UINT32_C(1) << ul;
        p.qtd_descartados++;
        break;
      case 'l':
        if ( parse_ulong( optarg, 43332, 54321, &ul ) == false ) {
          fprintf( stderr,
                   "\nERRO! Parametro de distribuicao nas linhas "
                   "'-l%s' invalido!\n\n",
                   optarg );
          return 1;
        }
        switch ( ul ) {
          case 43332: p.dl43332 = true; break;
          case 44322: p.dl44322 = true; break;
          case 44331: p.dl44331 = true; break;
          case 53322: p.dl53322 = true; break;
          case 54321: p.dl54321 = true; break;
          default:
            fprintf( stderr,
                     "\nERRO! Parametro de Distribuicao dos Numeros nas "
                     "Linhas '-l%s' possui argumento invalido!\n\n",
                     optarg );
            return 1;
        }
        break;
      case 'c':
        if ( parse_ulong( optarg, 43332, 54321, &ul ) == false ) {
          fprintf( stderr,
                   "\nERRO! Parametro de distribuicao nas colunas '-c%s' "
                   "invalido!\n\n",
                   optarg );
          return 1;
        }
        switch ( ul ) {
          case 43332: p.dc43332 = true; break;
          case 44322: p.dc44322 = true; break;
          case 44331: p.dc44331 = true; break;
          case 53322: p.dc53322 = true; break;
          case 54321: p.dc54321 = true; break;
          default:
            fprintf( stderr,
                     "\nERRO! Parametro de Distribuicao dos Numeros "
                     "nas Colunas '-c%s' possui argumento invalido!\n\n",
                     optarg );
            return 1;
        }
        break;
      case 's':
        if ( parse_ulong( optarg, 2, QTD_NUM_SORTEADOS, &ul ) == false ) {
          fprintf( stderr,
                   "\nERRO! Parametro de sequencia minima dos numeros "
                   "sorteados '-s%s' invalido!\n\n",
                   optarg );
          return 1;
        }
        p.seq_min_sorteados = (uint8_t)ul;
        break;
      case 'e':
        if ( parse_ulong( optarg, 2, QTD_NUM_NAO_SORTEADOS, &ul ) == false ) {
          fprintf( stderr,
                   "\nERRO! Parametro de sequencia minima dos numeros nao "
                   "sorteados '-e%s' invalido!\n\n",
                   optarg );
          return 1;
        }
        p.seq_min_descartados = (uint8_t)ul;
        break;
      case 'r':
        if ( parse_ulong( optarg, 1, LF_MAX_PRIME, &ul ) == false ) {
          fprintf( stderr,
                   "\nERRO! Parametro de numero primo MINIMO "
                   "'-r%s' invalido!\n\n",
                   optarg );
          return 1;
        }
        if ( p.primo_min == UNDEFINED_VALUE )
          p.primo_min = (int8_t)ul;
        else {
          fprintf( stderr,
                   "\nERRO! Parametro '-%c' precisa ser unico!\n\n",
                   caractere );
          return 1;
        }
        break;
      case 'm':
        if ( parse_ulong( optarg, 0, LF_MAX_PRIME, &ul ) == false ) {
          fprintf( stderr,
                   "\nERRO! Parametro de numero primo MAXIMO "
                   "'-m%s' invalido!\n\n",
                   optarg );
          return 1;
        }
        if ( p.primo_max == UNDEFINED_VALUE )
          p.primo_max = (int8_t)ul;
        else {
          fprintf( stderr,
                   "\nERRO! Parametro '-%c' precisa ser unico!\n\n",
                   caractere );
          return 1;
        }
        break;
      case '?':
        if ( ( optopt == 'n' ) || ( optopt == 'p' ) || ( optopt == 'a' ) ||
             ( optopt == 'i' ) || ( optopt == 'x' ) || ( optopt == 'f' ) ||
             ( optopt == 'd' ) || ( optopt == 'l' ) || ( optopt == 'c' ) ||
             ( optopt == 's' ) || ( optopt == 'e' ) || ( optopt == 'r' ) ||
             ( optopt == 'm' ) )
          fprintf(stderr, "\nOpcao '-%c' exige um argumento.\n\n", optopt );
        else if ( isprint(optopt) )
          fprintf( stderr, "Unknown option '-%c'.\n", optopt );
        else
          fprintf ( stderr,
                    "Unknown option character '\\x%x'.\n",
                    (unsigned)optopt );
        return 1;
      default:
        fprintf( stderr, "\nERRO! Parametro invalido!\n\n" );
        return 1;
    }
  }

  if ( p.soma_min > 0 && p.soma_max > 0 && p.soma_min > p.soma_max ) {
    fprintf( stderr,
             "\nERRO! \"-i (%d)\" maior que \"-x (%d)\"!\n\n",
             p.soma_min, p.soma_max );
    return 1;
  }

  if ( p.primo_min != UNDEFINED_VALUE && p.primo_max != UNDEFINED_VALUE &&
       p.primo_min > p.primo_max ) {
    fprintf( stderr,
             "\nERRO! \"-r (%d)\" maior que \"-m (%d)\"!\n\n",
             p.primo_min, p.primo_max );
    return 1;
  }

  if ( ( p.mask_fixos & p.mask_descartados ) != 0 ) {
    fprintf( stderr,
             "\nERRO! Numero %d esta em -f e em -d!\n\n",
             __builtin_ctz( p.mask_fixos & p.mask_descartados ) );
    return 1;
  }

  if ( p.qtd_comb_randomicas > 0 ) {
    total_possivel = binomial( NUM_MAXIMO - p.qtd_fixos - p.qtd_descartados,
                               QTD_NUM_SORTEADOS - p.qtd_fixos );
    if ( p.qtd_comb_randomicas > total_possivel ) {
      fprintf( stderr,
               "\nERRO! '-n' (%lu) maior que o total de %lu combinacoes "
               "possiveis com os parametros \"-f\" e/ou \"-d\" informados!\n\n",
               (unsigned long)p.qtd_comb_randomicas,
               (unsigned long)total_possivel );
      return 1;
    }
  }

  tmp_node = create_node();

  if ( p.qtd_comb_randomicas > 0 ) {
    usi = 0; // contador de combinacoes randomicas validas
    
    // faz o filtro pela quatidade de combinacoes randomicas geradas pelos sistema,
    // ignorando o arquito matriz

    while ( usi < p.qtd_comb_randomicas )
    {
      // Gera uma sequencia randomica valida
      get_valid_sequence( tmp_node );
      
      // qsort the tmp_node
      qsort( tmp_node->a_lt,
             QTD_NUM_SORTEADOS,
             sizeof( tmp_node->a_lt[0] ),
             qsort_asc );

      calc_stats( tmp_node, p.ultimo_concurso );

      // combinacao reprovada por algum filtro
      if ( ticket_ok( tmp_node, &p ) == false )
        continue;

      // testa se esta combinacao valida ja existe no array
      if ( push_node_end( &head, tmp_node, true ) ) {
        usi++;
        // imprime na tela essa combinacao valida após encontra-la
        print_node( tmp_node, &p );
      }
    }
  } else {
    if ( optind == argc ) {
      fprintf( stderr, "\nERRO! [arq matriz] nao especificado!\n\n" );
      return 1;
    }
    
    // opening file for reading
    fp = fopen( argv[ optind ], "r" );
    if ( fp == NULL ) {
      perror( "fopen(): Erro ao abrir o arquivo matriz!" );
      return 1;
    }

    while ( fgets( str, sizeof( str ), fp ) != NULL ) {
      /* linha maior que o buffer: sem '\n' e nao eh o fim do arquivo */
      if ( strchr( str, '\n' ) == NULL && !feof( fp ) ) {
        fprintf( stderr,
                 "\nERRO! Linha muito longa no arquivo '%s'!\n\n",
                 argv[optind] );
        fclose( fp );
        return 1;
      }
      /* ignora linha vazia */
      if ( str[ strspn( str, " \t\r\n" ) ] == '\0' )
        continue;

      rc = parse_line( str, tmp_node );
      if ( rc != LINE_OK ) {
        fclose( fp );
        switch ( rc ) {
          case LINE_RANGE:
            fprintf( stderr,
                     "\nERRO! Numero fora do escopo da %s (%d a %d)!\n\n",
                     NOM, NUM_MINIMO, NUM_MAXIMO );
            break;
          case LINE_DUP:
            fprintf( stderr,
                     "\nERRO! Numero repetido na linha do arquivo '%s'!\n\n",
                     argv[optind] );
            break;
          default:
            fprintf( stderr,
                     "\nERRO! Arquivo \"%s\" em formato errado!\n\n",
                     argv[optind] );
        }
        return 1;
      }

      qsort( tmp_node->a_lt, QTD_NUM_SORTEADOS,
             sizeof( tmp_node->a_lt[0] ), qsort_asc );
      calc_stats( tmp_node, p.ultimo_concurso );
      if ( ticket_ok( tmp_node, &p ) )
        push_node_end( &head, tmp_node, false );
    }
    fclose( fp );
    print_list( head, &p );
  }
  free( tmp_node );
  free_list( &head );

  return 0;
}
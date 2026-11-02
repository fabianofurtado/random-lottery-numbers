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
 *   * 2026-11-01 - Correcao no RNG
 *   * 2019-06-04 - Correcao de bugs
 *                - Inclusao do parametro "a11"
 *   * 2019-05-15 - Uso de variavel static para parametros
 *                - Inclusao da SOMA na saida
 *                - Saida com combinacoes unicas quando se utiliza numeros randomicos
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


typedef struct {
  int numero; /* por questoes de compatibilidade com a libconfig, estou usando
                 int mas gostaria de estar usando unsigned short int */
  uint8_t a_dezenas[QTD_NUM_SORTEADOS];
} concurso_T;

typedef struct node {
  uint8_t a_lottery_ticket[QTD_NUM_SORTEADOS];
  struct node * next;
} node_T;

typedef struct {
  bool p6, p7, p8, p9,
       a7, a8, a9, a10, a11,
       r4, r5, r6, r7,
       dl43332, dl44322, dl44331, dl53322, dl54321,
       dc43332, dc44322, dc44331, dc53322, dc54321;
  concurso_T ultimo_concurso;
  uint8_t a_fixos[NUM_MAX_FIXOS+1],             // +1 for "\0" to strlen()
          a_descartados[NUM_MAX_DESCARTADOS+1], // +1 for "\0" to strlen()
          seq_min_sorteados,
          seq_min_descartados;
  uint16_t soma_min,
           soma_max;
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


static void
get_valid_sequence( node_T *node ) // Baseado em Fisher-Yates
{
  uint32_t i, /* contador do laco e fronteira do baralho. Marca quantas cartas
                 ja foram sorteadas (0..i-1) e onde comeca a regiao ainda
                 disponivel (i..NUM_MAXIMO-1). Tambem e o indice onde o numero
                 sorteado vai parar, tanto em tmp_pool quanto em
                 a_lottery_ticket. */
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
    node->a_lottery_ticket[i] = tmp_pool[i];
  }
}


static void
print_list( node_T *head )
{
  uint8_t i;
  uint16_t soma = 0;
  node_T * current = head;

  while ( current != NULL ) {
    for ( i=0; i<QTD_NUM_SORTEADOS; i++ ) {
      printf( "%02d", current->a_lottery_ticket[i] );
      soma += current->a_lottery_ticket[i];
      if (i != (QTD_NUM_SORTEADOS-1)) {
        printf( " " );
      }
    }
    printf("    Soma = %d\n", soma);
    soma = 0;
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
pop_by_node( node_T **head, node_T *node_to_pop )
{
    node_T *current = *head;
    node_T *next_node = NULL;

    if ( (*head) == node_to_pop ) {
      pop_head_node(head);
      return;
    }

    while ( current->next != node_to_pop ) {
      /* return without deleting de node - index out of range */
      if (current->next == NULL) {
        return;
      }
      current = current->next;
    }
    
    next_node = current->next;
    current->next = next_node->next;
    free(next_node);
}


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
  new_node = calloc( 1, sizeof( node_T ) );
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

  memset( new_node, 0, sizeof( node_T ) );

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
         memcmp( current->a_lottery_ticket, tmp_node->a_lottery_ticket,
                 sizeof( current->a_lottery_ticket ) ) == 0 )
      return false;               /* duplicata: nao insere */
    last    = current;
    current = current->next;
  }

  node_T *new_node = create_node();   /* calloc: next ja vem NULL */
  memcpy( new_node->a_lottery_ticket, tmp_node->a_lottery_ticket,
          sizeof( new_node->a_lottery_ticket ) );

  if ( last == NULL )
    *head = new_node;             /* lista vazia */
  else
    last->next = new_node;

  return true;
}


static bool
somatorio_minimo_ok( node_T *current, uint16_t *min )
{
  uint8_t uc;
  uint16_t soma = 0;

  for (uc=0; uc<QTD_NUM_SORTEADOS; uc++)
    soma += current->a_lottery_ticket[uc];
  
  return ( soma >= *min ) ;
}

static bool
somatorio_maximo_ok( node_T *current, uint16_t *max )
{
  uint8_t uc;
  uint16_t soma = 0;

  for ( uc=0 ; uc < QTD_NUM_SORTEADOS; uc++ )
    soma += current->a_lottery_ticket[uc];
  
  return ( soma <= *max );
}


static bool
numeros_repetidos_concurso_anterior_ok( node_T *current, param_T *p )
{
  uint8_t uc1,
          uc2,
          num_repetidos = 0;


  for (uc1 = 0 ; uc1 < QTD_NUM_SORTEADOS ; uc1++ )
    for (uc2 = 0 ; uc2 < QTD_NUM_SORTEADOS ; uc2++ )
      if ( current->a_lottery_ticket[uc1] ==
           (p->ultimo_concurso).a_dezenas[uc2] ) {
        num_repetidos++;
        break;
      }
    
  return ( ( ( p->a7 )  && ( num_repetidos == 7 ) )  ||
           ( ( p->a8 )  && ( num_repetidos == 8 ) )  ||
           ( ( p->a9 )  && ( num_repetidos == 9 ) )  ||
           ( ( p->a10 ) && ( num_repetidos == 10 ) ) ||
           ( ( p->a11 ) && ( num_repetidos == 11 ) ) );
}


static bool
numero_par_ok( node_T *current, param_T *p )
{
  uint8_t uc,
          even = 0;

  for ( uc=0 ; uc < QTD_NUM_SORTEADOS ; uc++ )
    if ( current->a_lottery_ticket[uc] % 2 == 0 )
      even++;

  return ( ( ( p->p6 ) && ( even == 6 ) ) ||
           ( ( p->p7 ) && ( even == 7 ) ) ||
           ( ( p->p8 ) && ( even == 8 ) ) ||
           ( ( p->p9 ) && ( even == 9 ) ) );
}


static bool
filter_fixo( node_T *current, uint8_t *a_fixos )
{
  uint8_t i,
          j;
  size_t arr_len = strlen((char*)a_fixos);
  bool r = false;
  bool arr_check[NUM_MAX_FIXOS] = {false};

  if ( arr_len == 0 )
    return false;

  for ( i = 0; i < arr_len; i++ ) {
    for ( j = 0; j < QTD_NUM_SORTEADOS; j++ )
      if ( current->a_lottery_ticket[j] == a_fixos[i] ) {
        arr_check[i] = true;
        break;
      }
  }

  for ( i = 0; i < arr_len; i++ )
    if ( arr_check[i] == false ) {
      r = true;
      break;
    }
  
  return r;  
}


static bool
filter_descartado( node_T *current, uint8_t *a_descartados )
{
  uint8_t i,
          j;
  size_t arr_len = strlen((char*)a_descartados);
  bool r = false;
  bool arr_check[NUM_MAX_DESCARTADOS] = {false};
  
  if ( arr_len == 0 )
    return false;

  for ( i = 0 ; i < arr_len ; i++ ) {
    for ( j = 0; j < QTD_NUM_SORTEADOS; j++ )
      if ( current->a_lottery_ticket[j] == a_descartados[i] ) {
        arr_check[i] = true;
        break;
      }
  }

  for ( i=0 ; i < arr_len; i++ )
    if ( arr_check[i] ) {
      r = true;
      break;
    }
  
  return r;
}


static bool
seq_min_sorteados_ok( node_T *current, uint8_t *seq_min_sorteados )
{
  uint8_t uc,
          seq = 1, // contador da sequencia - inicializa-se com 1 pois sempre ha 1 seq de um numero. Ex: 02 04 06
          anterior = 0; // numero anterior para calculo da sequencia
  bool seq_ok = false;
                
  for ( uc=0 ; uc < QTD_NUM_SORTEADOS; uc++ ) {
    if ( anterior > 0 ) {
      if ( ( anterior + 1 ) == current->a_lottery_ticket[uc] ) {
        if ( ++seq == *seq_min_sorteados ) {
          seq_ok = true;
          break;
        }
      } else {
        seq = 1; // reinicia a sequencia
      }
    }
    anterior = current->a_lottery_ticket[uc];
  }
  
  return seq_ok;
}


static bool
seq_min_descartados_ok( node_T *current, uint8_t *seq_min_descartados )
{
  uint8_t uc,
          seq = 0; // contador da sequencia
  bool arr_check[NUM_MAXIMO] = {false};
  bool seq_ok = false;
                
  // inicializa o array com os numeros em suas respectivas posicoes
  // pois o calculo da sequencia dos descartados sera feito nesta base
  for (uc=0; uc<QTD_NUM_SORTEADOS; uc++)
    arr_check[current->a_lottery_ticket[uc]-1] =
      current->a_lottery_ticket[uc];

  if ( arr_check[0] == 0 )
    seq++;
  for ( uc=1; uc < (sizeof(arr_check)/sizeof(arr_check[0])); uc++ ) {
    // se uc>0, já posso usar a referencia uc-1 no array
    if ( arr_check[uc-1] > 0 )
      seq = 0;

    if ( arr_check[uc] == 0 )
      seq++;

    if ( seq >= *seq_min_descartados ) {
      seq_ok = true;
      break;
    }
  }

  return seq_ok;
}


static bool
is_prime( uint8_t num )
{
  uint8_t i;


  if ( num == 1 )
    return false;
  if ( ((num % 2) == 0) && (num > 2) )
    return false;
  for ( i = 3 ; i < num / 2 ; i+= 2 ) {
    if ( ( num % i ) == 0 )
      return false;
  }
  return true;
}

static bool
qtd_primo_ok( node_T *current, param_T *p )
{
  uint8_t uc,
          primo = 0;

  for ( uc=0 ; uc < QTD_NUM_SORTEADOS ; uc++ )
    if ( is_prime( current->a_lottery_ticket[uc] ) )
      primo++;

  return ( ( ( p->r4 ) && ( primo == 4 ) ) ||
           ( ( p->r5 ) && ( primo == 5 ) ) ||
           ( ( p->r6 ) && ( primo == 6 ) ) ||
           ( ( p->r7 ) && ( primo == 7 ) ) );
}


static bool
distribuicao_linha_ok( node_T *current, param_T *p )
{
  uint16_t padrao = 0;
  uint8_t  uc,
           linha[QTD_LINHAS] = { 0 }; // A lotofacil possui 5 linhas: 1..5, 6..10, 11..15, 16..20 e 21..25

                
  for ( uc=0 ; uc < QTD_NUM_SORTEADOS ; uc++ ) {
    if ( ( current->a_lottery_ticket[uc] < NUM_MINIMO ) ||
         ( current->a_lottery_ticket[uc] > NUM_MAXIMO ) ) {
      printf( "Invalid range in function 'distribuicao_linha_ok'.\n\n" );
      exit(1);
    }
    linha[(current->a_lottery_ticket[uc]-1)/5]++; // armazena em grupos de 5 em 5, comecando pelo index 0. Dai o uso do -1.
  }

  qsort( linha, QTD_LINHAS, sizeof(linha[0]), qsort_desc );

  // Esta transformacao so funciona para array com inteiros de 1 digito E
  // o numero inteiro final precisa 'caber' no tipo da variavel padrao.
  // Neste caso, o valor maximo seria 5 (1 digito).
  for ( uc = 0 ; uc < QTD_LINHAS ; uc++ )
    padrao = 10 * padrao + linha[uc];
  
  return ( ( ( p->dl43332 ) && ( padrao == 43332 ) ) ||
           ( ( p->dl44322 ) && ( padrao == 44322 ) ) ||
           ( ( p->dl44331 ) && ( padrao == 44331 ) ) ||
           ( ( p->dl53322 ) && ( padrao == 53322 ) ) ||
           ( ( p->dl54321 ) && ( padrao == 54321 ) ) );
}


static bool
distribuicao_coluna_ok( node_T *current, param_T *p )
{
  uint16_t padrao = 0;
  uint8_t uc,
          last_digit,
           coluna[QTD_COLUNAS] = { 0 }; // A lotofacil sena possui 5 colunas

  // Sempre as colunas terminam com o digito (last_digit):
  //   * 1a coluna: 1 ou 6
  //   * 1a coluna: 2 ou 7
  //   * 1a coluna: 3 ou 8
  //   * 1a coluna: 4 ou 9
  //   * 1a coluna: 5 ou 0

  // Este array coluna[5] vai armazenar as seguintes quantidades de ranges de numeros:
  //    coluna[5] = { 5|10|15|20|25, 1|6|11|16|21, 2|7|12|17|22, 3|8|13|18|23, 4|9|14|19|24 };

  for ( uc=0; uc<QTD_NUM_SORTEADOS; uc++ ) {
    if ( ( current->a_lottery_ticket[uc] < NUM_MINIMO ) ||
         ( current->a_lottery_ticket[uc] > NUM_MAXIMO ) ) {
      printf( "Invalid range in function 'distribuicao_coluna_ok'.\n\n" );
      exit(1);
    }
    last_digit = current->a_lottery_ticket[uc] % 10;
    if ( last_digit > 4 )
      last_digit = last_digit - 5; // Diminui 5 para ficar entre 0 e 4, indexes do array
    coluna[last_digit]++;
  }

  qsort( coluna, QTD_COLUNAS, sizeof(coluna[0]), qsort_desc );

  // Esta transformacao so funciona para array com inteiros de 1 digito E
  // o numero inteiro final precisa 'caber' no tipo da variavel padrao.
  // Neste caso, o valor maximo seria 5 (1 digito).
  for (uc = 0; uc < QTD_COLUNAS; uc++)
    padrao = 10 * padrao + coluna[uc];
  
  return ( ( ( p->dc43332 ) && ( padrao == 43332 ) ) ||
           ( ( p->dc44322 ) && ( padrao == 44322 ) ) ||
           ( ( p->dc44331 ) && ( padrao == 44331 ) ) ||
           ( ( p->dc53322 ) && ( padrao == 53322 ) ) ||
           ( ( p->dc54321 ) && ( padrao == 54321 ) ) );
}


static void
filter_list( node_T **head, param_T *p )
{
  node_T *current;

  if ( (p->p6) || (p->p7) || (p->p8) || (p->p9) ) {
    current = *head;
    while ( current != NULL ) {
      if ( numero_par_ok( current, p ) == false ) {
        pop_by_node( head, current );
        current = *head;
      } else {
        current = current->next;
      }
    }
  }

  if ( (p->a7) || (p->a8) || (p->a9) || (p->a10) || (p->a11) ) {
    current = *head;
    while ( current != NULL ) {
      if ( numeros_repetidos_concurso_anterior_ok( current, p ) == false ) {
        pop_by_node( head, current );
        current = *head;
      } else {
        current = current->next;
      }
    }
  }
    
  if ( p->soma_min > 0 ) {
    current = *head;
    while ( current != NULL ) {
      if ( somatorio_minimo_ok( current, &(p->soma_min) ) == false ) {
        pop_by_node( head, current );
        current = *head;
      } else {
        current = current->next;
      }
    }
  }

  if ( p->soma_max > 0 ) {
    current = *head;
    while ( current != NULL ) {
      if ( somatorio_maximo_ok( current, &(p->soma_max) ) == false ) {
        pop_by_node( head, current );
        current = *head;
      } else {
        current = current->next;
      }
    }
  }
  
  if ( strlen((char*)p->a_fixos) > 0 ) {
    current = *head;
    while ( current != NULL ) {
      if ( filter_fixo( current, p->a_fixos ) ) {
        pop_by_node( head, current );
        current = *head;
      } else {
        current = current->next;
      }
    }
  }

  if ( strlen((char*)p->a_descartados) > 0 ) {
    current = *head;
    while ( current != NULL ) {
      if ( filter_descartado( current, p->a_descartados ) ) {
        pop_by_node( head, current );
        current = *head;
      } else {
        current = current->next;
      }
    }
  }

  if ( p->seq_min_sorteados > 0 ) {
    current = *head;
    while ( current != NULL ) {
      if ( seq_min_sorteados_ok( current, &(p->seq_min_sorteados) ) == false ) {
        pop_by_node( head, current );
        current = *head;
      } else {
        current = current->next;
      }
    }
  }

  if ( p->seq_min_descartados > 0 ) {
    current = *head;
    while ( current != NULL ) {
      if ( seq_min_descartados_ok( current, &(p->seq_min_descartados) ) == false ) {
        pop_by_node( head, current );
        current = *head;
      } else {
        current = current->next;
      }
    }
  }

  if ( (p->r4) || (p->r5) || (p->r6) || (p->r7) ) {
    current = *head;
    while ( current != NULL ) {
      if ( qtd_primo_ok( current, p ) == false ) {
        pop_by_node( head, current );
        current = *head;
      } else {
        current = current->next;
      }
    }
  }

  if ( (p->dl43332) || (p->dl44322) || (p->dl44331) || (p->dl53322) || (p->dl54321) ) {
    current = *head;
    while ( current != NULL ) {
      if ( distribuicao_linha_ok( current, p ) == false ) {
        pop_by_node( head, current );
        current = *head;
      } else {
        current = current->next;
      }
    }
  }

  if ( (p->dc43332) || (p->dc44322) || (p->dc44331) || (p->dc53322) || (p->dc54321) ) {
    current = *head;
    while ( current != NULL ) {
      if ( distribuicao_coluna_ok( current, p ) == false ) {
        pop_by_node( head, current );
        current = *head;
      } else {
        current = current->next;
      }
    }
  }
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
    node->a_lottery_ticket[cnt++] = (uint8_t)v;
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
           "Uso: %s [FILTRO] -n[numero combinacoes randomicas]|[arq matriz]\n\n"
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
           "  -f<numero fixo>, fixa um numero nos jogos\n"
           "    * Obs: o numero maximo de fixos e %d.\n"
           "  -d<numero descartado>, descarta um numero nos jogos\n"
           "    * Obs: o numero maximo de descartados e %d.\n"
           "  -l<distribuicao>, Distribuicao dos Numeros nas Linhas\n"
           "    Possiveis valores para <distribuicao>:\n"
           "      * 43332\n"
           "      * 44322\n"
           "      * 44331\n"
           "      * 53322\n"
           "      * 54321\n"
           "  -c<distribuicao>, Distribuicao dos Numeros nas Colunas\n"
           "    Possiveis valores para <distribuicao>:\n"
           "      * 43332\n"
           "      * 44322\n"
           "      * 44331\n"
           "      * 53322\n"
           "      * 54321\n"
           "  -s<seq minimo> (2 a %d), filtra por uma sequencia minima de numeros sorteados.\n"
           "    * Exemplo: a combinacao ...02 04 06 07 08 09 13 14...\n"
           "      possui uma sequencia minima de 4 numeros sorteados  (06 07 08 e 09).\n"
           "      Neste caso, o parametro para filtro seria '-s4'.\n"
           "      Um bom parametro para o filtro seria '-s4' ou '-s5'.\n"
           "  -e<seq minimo> (2 a %d), filtra por uma sequencia minima de numeros nao sorteados.\n"
           "    * Exemplo: a combinacao ...02 04 06 07 08 09 13 14...\n"
           "      possui uma sequencia minima de 3 numeros nao sorteados (10 11 e 12).\n"
           "      Neste caso, o parametro para filtro seria '-e3'.\n"
           "      Um bom parametro para o filtro seria '-e3' ou '-e4'.\n"
           "  -r<qtd primos>, filtra por uma quantidade de numeros primos no sorteio.\n"
           "    * Obs: Possiveis valores para os parametros: -r4 -r5 -r6 -r7\n"
           "      Possiveis numeros primos para LF: 2, 3, 5, 7, 11, 13, 17, 19, 23\n"
           "      Estatisticamente, recomanda-se, em media, o uso de '-r5'\n\n",
           argv0,
           SOMA_MIN, SOMA_MAX,            /* -i */
           SOMA_MIN, SOMA_MAX,            /* -x */
           NUM_MAX_FIXOS,                 /* -f */
           NUM_MAX_DESCARTADOS,           /* -d */
           QTD_NUM_SORTEADOS,             /* -s */
           QTD_NUM_NAO_SORTEADOS );       /* -e */
}


int
main( int argc, char **argv )
{
  static param_T  p;
  static node_T  *head = NULL, /* Declare the head of the list.
                                  The head is used as a reference for all
                                  functions. */
                 *tmp_node = NULL;
  FILE           *fp;
  char           str[ MAX_LINE ];
  int            rc;  // return code
  uint8_t        c;   // contador do array de inteiros
  uint64_t       usi = 0,
                 total_possivel;
  unsigned long  ul;
  int            caractere;
  size_t         nf,
                 nd;


  if ( argc < 2 ) {
    print_help( argv[0] );
    return 1;
  }

  //memset( &p, 0, sizeof( param_T ) ); // retirado. p eh static
  opterr = 0;

  while ( ( caractere = getopt( argc, argv,
                                "p:a:i:x:f:d:l:c:s:e:r:n:" ) ) != -1 ) {
    switch ( caractere ) {
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
        if ( !parse_ulong( optarg, 7, 11, &ul ) ) {
          fprintf( stderr,
                   "\nERRO! Parametro \"numeros repetidos do concurso "
                   "anterior\" '-a%s' invalido!\n\n",
                   optarg );
          return 1;
        }
        switch ( ul ) {
          case  7: p.a7 = true; break;
          case  8: p.a8 = true; break;
          case  9: p.a9 = true; break;
          case 10: p.a10 = true; break;
          case 11: p.a11 = true; break;
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
        if ( memchr( p.a_fixos, (int)ul, strlen( (char*)p.a_fixos ) ) != NULL ){
          fprintf( stderr, "\nERRO! Numero fixo %lu repetido em -f!\n\n", ul );
          return 1;
        }
        if ( strlen( (char*)p.a_fixos ) + 1 > NUM_MAX_FIXOS ) {
          fprintf( stderr,
                   "\nERRO! Nao podem existir mais que %d numeros fixos!\n\n",
                   NUM_MAX_FIXOS );
          return 1;
        }
        p.a_fixos[ strlen( (char*)p.a_fixos ) ] = (uint8_t)ul;
        break;
      case 'd':
        if ( parse_ulong( optarg, NUM_MINIMO, NUM_MAXIMO, &ul ) == false ) {
          fprintf( stderr,
                   "\nERRO! Parametro '-d%s' invalido! "
                   "Deve estar entre %d e %d.\n\n",
                   optarg, NUM_MINIMO, NUM_MAXIMO );
          return 1;
        }
        if ( memchr( p.a_descartados,
                     (int)ul,
                     strlen( (char*)p.a_descartados ) ) != NULL ) {
          fprintf( stderr,
                   "\nERRO! Numero descartado %lu repetido em -d!\n\n",
                   ul );
          return 1;
        }
        if ( strlen( (char*)p.a_descartados ) + 1 > NUM_MAX_DESCARTADOS ) {
          fprintf( stderr,
                   "\nERRO! Nao podem existir mais que %d numeros "
                   "descartados!\n\n",
                   NUM_MAX_DESCARTADOS );
          return 1;
        }
        p.a_descartados[strlen((char*)p.a_descartados)] = (uint8_t)ul;
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
        if ( parse_ulong( optarg, 4, 7, &ul ) == false ) {
          fprintf( stderr,
                   "\nERRO! Parametro de numero primo '-r%s' invalido!\n\n",
                   optarg );
          return 1;
        }
        switch ( ul ) {
          case 4: p.r4 = true; break;
          case 5: p.r5 = true; break;
          case 6: p.r6 = true; break;
          case 7: p.r7 = true; break;
          default:
            fprintf( stderr, 
                     "\nERRO! Parametro de numero primo '-r%s' invalido!\n\n",
                     optarg );
            return 1;
        }
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
      case '?':
        if ( ( optopt == 'p' ) || ( optopt == 'a' ) || ( optopt == 'i' ) ||
             ( optopt == 'x' ) || ( optopt == 'f' ) || ( optopt == 'd' ) ||
             ( optopt == 'l' ) || ( optopt == 'c' ) || ( optopt == 's' ) ||
             ( optopt == 'e' ) || ( optopt == 'r' ) || ( optopt == 'n' ) )
          fprintf (stderr, "\nOpcao '-%c' exige um argumento.\n\n", optopt);
        else if (isprint (optopt))
          fprintf (stderr, "Unknown option '-%c'.\n", optopt);
        else
          fprintf (stderr, "Unknown option character '\\x%x'.\n", optopt);
        return 1;
      default:
        fprintf( stderr, "\nERRO! Parametro invalido!\n\n" );
        return 1;
    }
  }

  if ( p.soma_min > 0 && p.soma_max > 0 && p.soma_min > p.soma_max ) {
    fprintf( stderr,
             "\nERRO! \"-i (%u)\" maior que \"-x (%u)\"!\n\n",
             p.soma_min, p.soma_max );
    return 1;
  }

  for ( c = 0 ; p.a_fixos[c] != 0 ; c++ ) {
    if ( memchr( p.a_descartados, p.a_fixos[c],
                 strlen( (char*)p.a_descartados ) ) != NULL ) {
      fprintf( stderr,
               "\nERRO! Numero %u esta em -f e em -d!\n\n",
               p.a_fixos[c] );
      return 1;
    }
  }

  if ( p.qtd_comb_randomicas > 0 ) {
    nf = strlen( (char*)p.a_fixos );
    nd = strlen( (char*)p.a_descartados );
    total_possivel = binomial( NUM_MAXIMO - nf - nd, QTD_NUM_SORTEADOS - nf );
    if ( p.qtd_comb_randomicas > total_possivel ) {
      fprintf( stderr,
               "\nERRO! '-n' (%lu) maior que o total de %lu jogos possiveis "
               "com os parametros \"-f\" e/ou \"-d\" informados!\n\n",
               (unsigned long)p.qtd_comb_randomicas,
               (unsigned long)total_possivel );
      return 1;
    }
  }

  if ( p.a7 || p.a8 || p.a9 || p.a10 || p.a11 )
     read_conf_file(&(p.ultimo_concurso));

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
      qsort( tmp_node->a_lottery_ticket,
             QTD_NUM_SORTEADOS,
             sizeof(tmp_node->a_lottery_ticket[0]),
             qsort_asc );

      filter_list( &tmp_node, &p );
      
      // sequencia com filtro invalido
      if ( tmp_node == NULL )
        tmp_node = create_node();
      else {
        // testa se esta combinacao valida ja existe no array
        if ( push_node_end( &head, tmp_node, true ) ) {
          usi++;
          // imprime na tela essa combinacao valida após encontra-la
          print_list(tmp_node);
        }
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

      qsort( tmp_node->a_lottery_ticket, QTD_NUM_SORTEADOS,
             sizeof( tmp_node->a_lottery_ticket[0] ), qsort_asc );
      push_node_end( &head, tmp_node, false );
    }
    fclose( fp );
    filter_list( &head, &p );
    print_list(head);
  }
  pop_head_node(&tmp_node);
  free_list(&head);

  return 0;
}
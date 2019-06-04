/*
 * 2019-01-16 by Fabiano Furtado
 * 
 * Based on: http://www.learn-c.org/en/Linked_lists
 * 
 * Generate the random list number:
 *   https://www.random.org/integer-sets/?sets=1666&num=6&min=1&max=50&sort=on&order=values&format=plain&rnd=new
 * 
 * Compile with gcc:
 *   gcc -Wall -O3 ds_filter.c -o ds_filter
 *
 * Revisions:
 *   2019-06-04 - Buf fix on function get_valid_sequence()
 *   2019-05-16 - Add -n parameter do generate random numbers
 *   2019-02-28 - Version 2
 *              - Add "soma" on result
 *              - use static params variable with struct
 *   2019-01-31 - Add -c parameter
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>


#define NUM_MINIMO           1
#define NUM_MAXIMO           50
#define QTD_NUM_SORTEADOS    6
#define QTD_LINHAS           5
#define QTD_COLUNAS          10
#define NUM_MAX_FIXOS        QTD_NUM_SORTEADOS-1
#define NUM_MAX_DESCARTADOS  NUM_MAXIMO-QTD_NUM_SORTEADOS-1
#define MAX_LINE             64


typedef struct node {
  unsigned char lottery_ticket_arr[QTD_NUM_SORTEADOS];
  struct node * next;
} node_t;

typedef struct {
  bool p1, p2, p3, p4,
       dl22110,  dl32100,  dl21111,  dl31110,  dl22200,
       dc211110, dc221100, dc111111, dc311100, dc321000;
  unsigned char arr_fixos[NUM_MAX_FIXOS+1], // +1 for "\0", to strlen
                arr_descartados[NUM_MAX_DESCARTADOS+1],
                seq_min_sorteados;
  unsigned short int soma_min,
                     soma_max,
                     qtd_comb_randomicas;
} param_t;


int qsort_asc (const void * a, const void * b) {
   return ( *(unsigned char*)a - *(unsigned char*)b );
}


int qsort_desc (const void * a, const void * b) {
   return ( *(unsigned char*)b - *(unsigned char*)a );
}


void get_valid_sequence( node_t *node ) {
  unsigned char i,
                arr_tmp[NUM_MAXIMO];
                
  bool seq_valida = false;

  while ( seq_valida == false ) {
    // zera o array temporario
    memset(arr_tmp, 0, sizeof(arr_tmp));

    // insere numeros aleatorios na sequencia
    for (i=0; i < QTD_NUM_SORTEADOS; i++)
      node->lottery_ticket_arr[i] = (rand() % (NUM_MAXIMO - NUM_MINIMO + 1)) + NUM_MINIMO;
  
    // verifica se a sequencia possui numeros repetidos
    for ( i=0; i < QTD_NUM_SORTEADOS; i++ ) {
      if ( arr_tmp[node->lottery_ticket_arr[i]-1] == 1 ) {
        seq_valida = false;
        break;
      }
      else {
        seq_valida = true;
        arr_tmp[node->lottery_ticket_arr[i]-1]++;
      }
    }
  }
}


void print_list( node_t *head ) {
  unsigned char i;
  unsigned short int soma = 0;
  node_t * current = head;

  while ( current != NULL ) {
    for ( i=0; i<QTD_NUM_SORTEADOS; i++ ) {
      printf( "%02d", current->lottery_ticket_arr[i] );
      soma += current->lottery_ticket_arr[i];
      if (i != (QTD_NUM_SORTEADOS-1)) {
        printf( " " );
      }
    }
    printf("    Soma = %d\n", soma);
    soma = 0;
    current = current->next;
  }
}


void pop_head_node(node_t **head) {
  node_t * next_node = NULL;

  if (*head == NULL) {
     return;
  }

  next_node = (*head)->next;
  free(*head);
  *head = next_node;
}


void pop_by_index(node_t **head, unsigned char index) {
    unsigned char i = 0;
    node_t * current = *head;
    node_t * temp_node = NULL;

    if (index == 0) {
      pop_head_node(head);
      return;
    }

    for (i = 0; i < index-1; i++) {
      /* return without deleting de node - index out of range */
      if (current->next == NULL) {
        return;
      }
      current = current->next;
    }

    temp_node = current->next;
    current->next = temp_node->next;
    free(temp_node);
}


void pop_by_node( node_t **head, node_t *node_to_pop ) {
    node_t * current = *head;
    node_t * next_node = NULL;

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


void free_list(node_t **head) {
  while ( *head != NULL ) {
    pop_head_node(head);
  }
}


node_t* create_node() {
  node_t * new_node = NULL;
  new_node = malloc(sizeof(node_t));
  if (new_node == NULL) {
    perror("create_node(): malloc cannot alocate memory!\n");
    exit(1);
  }    
  return new_node;
}

/*
node_t* create_clean_node() {
  node_t *new_node = create_node();

  memset(new_node, 0, sizeof(node_t));

  return new_node;
}
*/

/* add an item to the end of the list */
bool push_node_end( node_t **head, node_t *tmp_node, bool unique_combination ) {
  bool is_unique_combination = true;

  /* alocate node if the list is empty */
  if ( *head == NULL ) {
    *head = create_node();
    memcpy((*head)->lottery_ticket_arr, tmp_node->lottery_ticket_arr, sizeof((*head)->lottery_ticket_arr));
    (*head)->next = NULL;
  }
  else {
    node_t * current = NULL;
    current = *head;
    /* search for the last node */
    while (current->next != NULL) {
      if ( ( unique_combination ) && ( memcmp( current->lottery_ticket_arr, tmp_node->lottery_ticket_arr, sizeof((*head)->lottery_ticket_arr) ) == 0 ) )
        is_unique_combination = false;
      current = current->next;
    }
    /* now we can add a new variable */
    if ( is_unique_combination ) {
      current->next = create_node();
      memcpy(current->next->lottery_ticket_arr, tmp_node->lottery_ticket_arr, sizeof(current->next->lottery_ticket_arr));
      current->next->next = NULL;
    }
  }
  
  return is_unique_combination;
}

bool somatorio_minimo_ok( node_t *current, unsigned short int *min ) {
  unsigned char uc;
  unsigned short int soma = 0;
  
  for (uc=0; uc<QTD_NUM_SORTEADOS; uc++)
    soma += current->lottery_ticket_arr[uc];

  return ( soma >= *min ) ;
}

bool somatorio_maximo_ok( node_t *current, unsigned short int *max ) {
  unsigned char uc;
  unsigned short int soma = 0;

  for (uc=0; uc<QTD_NUM_SORTEADOS; uc++)
    soma += current->lottery_ticket_arr[uc];
  
  return ( soma <= *max );
}

bool numero_par_ok( node_t *current, param_t *p ) {
  unsigned char uc,
                even = 0;

  for (uc=0; uc<QTD_NUM_SORTEADOS; uc++)
    if ( current->lottery_ticket_arr[uc] % 2 == 0 )
      even++;

  return ( ( ( p->p1 ) && ( even == 1 ) ) ||
           ( ( p->p2 ) && ( even == 2 ) ) ||
           ( ( p->p3 ) && ( even == 3 ) ) ||
           ( ( p->p4 ) && ( even == 4 ) ) );
}


bool filter_fixo( node_t *current, unsigned char arr_fixos[] ) {
  unsigned char i,
                j;
  size_t arr_len = strlen((char*)arr_fixos);
  bool r = false;
  bool arr_check[NUM_MAX_FIXOS] = {false};

  if ( arr_len == 0 )
    return false;

  for (i=0; i<arr_len; i++) {
    for (j=0; j<QTD_NUM_SORTEADOS; j++)
      if ( current->lottery_ticket_arr[j] == arr_fixos[i] ) {
        arr_check[i] = true;
        break;
      }
  }

  for (i=0; i<arr_len; i++)
    if ( arr_check[i] == false ) {
      r = true;
      break;
    }
  
  return r;  
}


bool filter_descartado( node_t *current, unsigned char arr_descartados[] ) {
  unsigned char i,
                j;
  size_t arr_len = strlen((char*)arr_descartados);
  bool r = false;
  bool arr_check[NUM_MAX_DESCARTADOS] = {false};
  
  if ( arr_len == 0 )
    return false;

  for (i=0; i<arr_len; i++) {
    for (j=0; j<QTD_NUM_SORTEADOS; j++)
      if ( current->lottery_ticket_arr[j] == arr_descartados[i] ) {
        arr_check[i] = true;
        break;
      }
  }

  for (i=0; i<arr_len; i++)
    if ( arr_check[i] ) {
      r = true;
      break;
    }
  
  return r;  
}


bool seq_min_sorteados_ok( node_t *current, unsigned char *seq_min_sorteados ) {
  unsigned char uc,
                seq = 1, // contador da sequencia - inicializa-se com 1 pois sempre ha 1 seq de um numero. Ex: 02 04 06
                anterior = 0; // numero anterior para calculo da sequencia
  bool seq_ok = false;
                
  for (uc=0; uc<QTD_NUM_SORTEADOS; uc++) {
    if ( anterior > 0 ) {
      if ( ( anterior + 1 ) == current->lottery_ticket_arr[uc] ) {
        if ( ++seq == *seq_min_sorteados ) {
          seq_ok = true;
          break;
        }
      } else
        seq = 1; // reinicia a sequencia
    }
    anterior = current->lottery_ticket_arr[uc];
  }
  
  return seq_ok;
}


bool distribuicao_linha_ok( node_t *current, param_t *p ) {
  unsigned short int padrao = 0;
  unsigned char uc,
                linha[QTD_LINHAS] = { 0 }; // A dupla sena possui 5 linhas: 1..10, 11..20, 21..30, 31..40 e 41..50

                
  for (uc=0; uc<QTD_NUM_SORTEADOS; uc++) {
    if ( ( current->lottery_ticket_arr[uc] < NUM_MINIMO ) || ( current->lottery_ticket_arr[uc] > NUM_MAXIMO ) ) {
      printf( "Invalid range in function 'distribuicao_linha_ok'.\n\n" );
      exit(1);
    }
    linha[(current->lottery_ticket_arr[uc]-1)/10]++; // armazena em grupos de 10 em 10, comecando pelo index 0. Dai o uso do -1.
  }

  qsort( linha, QTD_LINHAS, sizeof(linha[0]), qsort_desc );

  // Esta transformacao so funciona para array com inteiros de 1 digito E
  // o numero inteiro final precisa 'caber' no typo da variavel padrao.
  for (uc = 0; uc < QTD_LINHAS; uc++)
    padrao = 10 * padrao + linha[uc];
  
  return ( ( ( p->dl22110 ) && ( padrao == 22110 ) ) ||
           ( ( p->dl32100 ) && ( padrao == 32100 ) ) ||
           ( ( p->dl21111 ) && ( padrao == 21111 ) ) ||
           ( ( p->dl31110 ) && ( padrao == 31110 ) ) ||
           ( ( p->dl22200 ) && ( padrao == 22200 ) ) );
}



bool distribuicao_coluna_ok( node_t *current, param_t *p ) {
  unsigned long padrao = 0;
  unsigned char uc,
                last_digit,
                coluna[QTD_COLUNAS] = { 0 }; // A dupla sena possui 10 colunas


  // Sempre as colunas terminam com o digito (last_digit):
  //   * 1a coluna: 1
  //   * 1a coluna: 2
  //   * 1a coluna: 3
  //   * 1a coluna: 4
  //   * 1a coluna: 5
  //     .......

  for (uc=0; uc<QTD_NUM_SORTEADOS; uc++) {
    if ( ( current->lottery_ticket_arr[uc] < NUM_MINIMO ) || ( current->lottery_ticket_arr[uc] > NUM_MAXIMO ) ) {
      printf( "Invalid range in function 'distribuicao_coluna_ok'.\n\n" );
      exit(1);
    }
    last_digit = current->lottery_ticket_arr[uc] % 10;
    coluna[last_digit]++;
  }
                
  qsort( coluna, QTD_COLUNAS, sizeof(coluna[0]), qsort_desc );

  // Esta transformacao so funciona para array com inteiros de 1 digito E
  // o numero inteiro final precisa 'caber' no typo da variavel padrao.
  for (uc = 0; uc < QTD_COLUNAS; uc++)
    padrao = 10 * padrao + coluna[uc];
  
  return ( ( ( p->dc211110 ) && ( padrao == 2111100000 ) ) ||
           ( ( p->dc221100 ) && ( padrao == 2211000000 ) ) ||
           ( ( p->dc111111 ) && ( padrao == 1111110000 ) ) ||
           ( ( p->dc311100 ) && ( padrao == 3111000000 ) ) ||
           ( ( p->dc321000 ) && ( padrao == 3210000000 ) ) );
}


void filter_list( node_t **head, param_t *p ) {
  node_t *current;

  if ( (p->p1) || (p->p2) || (p->p3) || (p->p4) ) {
    current = *head;
    while ( current != NULL ) {
      if ( numero_par_ok( current, p ) == false ) {
        pop_by_node( head, current );
        current = *head;
      }
      else current = current->next;
    }
  }
  
  if ( p->soma_min > 0 ) {
    current = *head;
    while ( current != NULL ) {
      if ( somatorio_minimo_ok( current, &(p->soma_min) ) == false ) {
        pop_by_node( head, current );
        current = *head;
      }
      else current = current->next;
    }
  }
  

  if ( p->soma_max > 0 ) {
    current = *head;
    while ( current != NULL ) {
      if ( somatorio_maximo_ok( current, &(p->soma_max) ) == false ) {
        pop_by_node( head, current );
        current = *head;
      }
      else current = current->next;
    }
  }

  if ( strlen((char*)p->arr_fixos) > 0 ) {
    current = *head;
    while ( current != NULL ) {
      if ( filter_fixo( current, p->arr_fixos ) ) {
        pop_by_node( head, current );
        current = *head;
      }
      else current = current->next;
    }
  }

  if ( strlen((char*)p->arr_descartados) > 0 ) {
    current = *head;
    while ( current != NULL ) {
      if ( filter_descartado( current, p->arr_descartados ) ) {
        pop_by_node( head, current );
        current = *head;
      }
      else current = current->next;
    }
  }

  if ( p->seq_min_sorteados > 0 ) {
    current = *head;
    while ( current != NULL ) {
      if ( seq_min_sorteados_ok( current, &(p->seq_min_sorteados) ) == false ) {
        pop_by_node( head, current );
        current = *head;
      }
      else current = current->next;
    }
  }

  if ( (p->dl22110) || (p->dl32100) || (p->dl21111) || (p->dl31110) || (p->dl22200) ) {
    current = *head;
    while ( current != NULL ) {
      if ( distribuicao_linha_ok( current, p ) == false ) {
        pop_by_node( head, current );
        current = *head;
      }
      else current = current->next;
    }
  }

  if ( (p->dc211110) || (p->dc221100) || (p->dc111111) || (p->dc311100) || (p->dc321000) ) {
    current = *head;
    while ( current != NULL ) {
      if ( distribuicao_coluna_ok( current, p ) == false ) {
        pop_by_node( head, current );
        current = *head;
      }
      else current = current->next;
    }
  }
}


void print_help( const char argv0[] ) {
  printf( "\n%s: Numero de parametros invalidos.\n\n", argv0 );
  printf( "Uso: %s [FILTRO] -n[numero combinacoes randomicas]|[arquivo matriz]\n\n", argv0 );
  printf( "Onde [FILTRO]:\n" );
  printf( "  -p <quantidade de pares>\n" );
  printf( "     Exemplo: -p1, filtra por 1 par   (e 5 impares)\n" );
  printf( "              -p2, filtra por 2 pares (e 4 impares)\n" );
  printf( "              -p3, filtra por 3 pares (e 3 impares)\n" );
  printf( "              -p4, filtra por 4 pares (e 2 impares)\n" );
  printf( "  -i<min>, soma MINIMA de todos os numeros\n");
  printf( "  -x<max>, soma MAXIMA de todos os numeros\n");
  printf( "    * Estatisticamente, recomanda-se <min> = 129 e <max> = 155\n" );
  printf( "  -f<numero fixo>, fixa um numero nos jogos\n");
  printf( "    * O numero maximo de fixos e %d.\n", NUM_MAX_FIXOS );
  printf( "  -d<numero descartado>, descarta um numero nos jogos\n");
  printf( "    * O numero maximo de descartados e %d.\n", NUM_MAX_DESCARTADOS );
  printf( "  -l<distribuicao>, Distribuicao dos Numeros nas Linhas\n");
  printf( "    Possiveis valores para <distribuicao>:\n");
  printf( "      * 22110\n");
  printf( "      * 32100\n");
  printf( "      * 21111\n");
  printf( "      * 31110\n");
  printf( "      * 22200\n");
  printf( "  -c<distribuicao>, Distribuicao dos Numeros nas Colunas\n");
  printf( "    Possiveis valores para <distribuicao>:\n");
  printf( "      * 211110\n");
  printf( "      * 221100\n");
  printf( "      * 111111\n");
  printf( "      * 311100\n");
  printf( "      * 321000\n");
  printf( "  -s<seq minimo>, filtra por uma sequencia minima de numeros sorteados.\n");
  printf( "    * Exemplo: a combinacao ...02 14 15 16 20...\n" );
  printf( "      possui uma sequencia minima de 3 numeros sorteados  (14 15 e 16).\n" );
  printf( "      Neste caso, o parametro para filtro seria '-s4'.\n" );
}

int main( int argc, char *argv[] ) {
  
  static param_t p;
  memset( &p, 0, sizeof(param_t) ); // clear full struct

  static node_t *head = NULL,
                *tmp_node = NULL;

  unsigned char //new_ticket[QTD_NUM_SORTEADOS],
                //arr_fixos[NUM_MAX_FIXOS+1] = { 0 }, // +1 for "\0", to strlen
                //arr_descartados[NUM_MAX_DESCARTADOS+1] = { 0 }, // +1 for "\0", to strlen
                ln,
                n,   // numero lido do arquivo texto
                c;   // contador do array de inteiros

  FILE   *fp;
  char   str[ MAX_LINE ],
         *psRetorno;

  unsigned short int usi = 0;
         
  int caractere;

  /* at least, one filter argument is required */
  if ( argc < 3 ) {
    print_help( argv[0] );
    return 1;
  }
  
  opterr = 0;
  
  while ( ( caractere = getopt ( argc, argv, "p:i:x:f:d:l:c:s:n:" ) ) != -1 )
    switch ( caractere ) {
      case 'p':
        switch ( atoi( optarg ) ) {
          case 1: p.p1 = true; break;
          case 2: p.p2 = true; break;
          case 3: p.p3 = true; break;
          case 4: p.p4 = true; break;
          default:
            printf( "\nERRO! Parametro '-p%s' invalido!\n\n", optarg );
            return 1;
        }
        break;
      case 'i':
        p.soma_min = atoi( optarg );
        if ( p.soma_min == 0 ) {
            printf( "\nERRO! Parametro de soma '-i' possui argumento invalido!\n\n" );
            return 1;
        }
        break;
      case 'x':
        p.soma_max = atoi( optarg );
        if ( p.soma_max == 0 ) {
            printf( "\nERRO! Parametro de soma '-x' possui argumento invalido!\n\n" );
            return 1;
        }
        break;
      case 'f':
        c = atoi( optarg );
        if ( c == 0 ) {
            printf( "\nERRO! Parametro '-f%s' possui argumento invalido!\n\n", optarg );
            return 1;
        }
        if ( ( c < NUM_MINIMO ) || ( c > NUM_MAXIMO ) ) {
            printf( "\nERRO! Parametro '-f%s' possui argumento invalido!\n", optarg );
            printf( "O parametro deve estar entre %d e %d!\n\n", NUM_MINIMO, NUM_MAXIMO );
            return 1;
        }
        if ( strlen((char*)p.arr_fixos)+1 > NUM_MAX_FIXOS ) {
            printf( "\nERRO! Nao podem existir mais que %d numeros fixos!\n\n", NUM_MAX_FIXOS );
            return 1;
        }
        // update the array
        p.arr_fixos[strlen((char*)p.arr_fixos)] = c;
        break;
      case 'd':
        c = atoi( optarg );
        if ( c == 0 ) {
            printf( "\nERRO! Parametro '-d%s' possui argumento invalido!\n\n", optarg );
            return 1;
        }
        if ( ( c < NUM_MINIMO ) || ( c > NUM_MAXIMO ) ) {
            printf( "\nERRO! Parametro '-d%s' possui argumento invalido!\n", optarg );
            printf( "O parametro deve estar entre %d e %d!\n\n", NUM_MINIMO, NUM_MAXIMO );
            return 1;
        }
        if ( strlen((char*)p.arr_descartados)+1 > NUM_MAX_DESCARTADOS ) {
            printf( "\nERRO! Nao podem existir mais que %d numeros descartados!\n\n", NUM_MAX_DESCARTADOS );
            return 1;
        }
        // update the array
        p.arr_descartados[strlen((char*)p.arr_descartados)] = c;
        break;
      case 'l':
        switch ( atoi( optarg ) ) {
          case 22110: p.dl22110 = true; break;
          case 32100: p.dl32100 = true; break;
          case 21111: p.dl21111 = true; break;
          case 31110: p.dl31110 = true; break;
          case 22200: p.dl22200 = true; break;
          default:
            printf( "\nERRO! Parametro de Distribuicao dos Numeros nas Linhas '-l%s' possui argumento invalido!\n\n", optarg );
            return 1;
        }
        break;
      case 'c':
        switch ( atoi( optarg ) ) {
          case 211110: p.dc211110 = true; break;
          case 221100: p.dc221100 = true; break;
          case 111111: p.dc111111 = true; break;
          case 311100: p.dc311100 = true; break;
          case 321000: p.dc321000 = true; break;
          default:
            printf( "\nERRO! Parametro de Distribuicao dos Numeros nas Colunas '-l%s' possui argumento invalido!\n\n", optarg );
            return 1;
        }
        break;
      case 's':
        p.seq_min_sorteados = atoi( optarg );
        if ( p.seq_min_sorteados == 0 ) {
            printf( "\nERRO! Parametro de sequencia minima '-s' possui argumento invalido!\n\n" );
            return 1;
        }
        break;
      case 'n':
        p.qtd_comb_randomicas = atoi( optarg );
        if ( p.qtd_comb_randomicas == 0 ) {
            printf( "\nERRO! Parametro numero de combinacoes randomicas '-n' possui argumento invalido!\n\n" );
            return 1;
        }
        break;
      case '?':
        if ( ( optopt == 'p' ) || ( optopt == 'i' ) || ( optopt == 'x' ) || ( optopt == 'f' ) || ( optopt == 'd' ) ||
             ( optopt == 'l' ) || ( optopt == 'c' ) || ( optopt == 's' ) || ( optopt == 'n' ) )
          fprintf (stderr, "\nOpcao '-%c' exige um argumento.\n\n", optopt);
        else if (isprint (optopt))
          fprintf (stderr, "Unknown option '-%c'.\n", optopt);
        else
          fprintf (stderr, "Unknown option character '\\x%x'.\n", optopt);
        return 1;
      default:
        printf( "\nERRO! Parametro invalido!\n\n" );
        return 1;
    }
    
  tmp_node = create_node();

  if ( p.qtd_comb_randomicas > 0 ) {
    srand(time(0)); // inicializa a geracao de numeros randomicos com uma "semente"
    usi = 0; // contador de combinacoes randomicas validas
    
    // faz o filtro pela quatidade de combinacoes randomicas geradas pelos sistema,
    // ignorando o arquito matriz
    
    while ( usi < p.qtd_comb_randomicas ) {
      // Gera uma sequencia randomica valida
      get_valid_sequence ( tmp_node );
      
      // qsort the tmp_node
      qsort( tmp_node->lottery_ticket_arr, QTD_NUM_SORTEADOS, sizeof(tmp_node->lottery_ticket_arr[0]), qsort_asc );

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
        printf( "\nERRO! [arquivo matriz] nao especificado!\n\n" );
        return 1;
    }
    
    // opening file for reading
    fp = fopen( argv[ optind ], "r" );
    if ( fp == NULL ) {
        perror( "\nfopen(): Erro ao abrir o arquivo matriz!\n\n" );
        return 1;
    }

    // File Reading Loop
    while ( fgets ( str, sizeof( str ), fp ) != NULL ) {
      // Retirando o caractere de retorno da string
      ln = strlen( str ) - 1;
      if ( str[ln] == '\n' )
      str[ln] = '\0';
      // Realizando o Split através dos espacos em branco
      // Primeira pesquisa por espaço em branco.
      psRetorno = strtok( str, " " );
      usi = 0;
      while ( psRetorno ) {
        n = atoi( psRetorno );
        if ( ( n < NUM_MINIMO ) || ( n > NUM_MAXIMO ) ) {
            printf( "\nERRO! Numero '%d' fora do escopo da Dupla Sena!\n\n", n );
            return 1;
        }
        if ( n > 0 )
            tmp_node->lottery_ticket_arr[usi++] = n;
        // Nas próximas chamadas, deve-se passar um ponteiro “nulo”.
        //   Isto “indica” para a função que a pesquisa deve continuar
        //   no ponteiro anterior.
        psRetorno = strtok( NULL, " " ); // Demais pesquisas por espaço em branco
        if ( psRetorno == NULL ) {
            if ( usi != QTD_NUM_SORTEADOS ) {
            printf( "\nERRO! Arquivo '%s' em formato errado!\n\n", argv[ optind ] );
            return 1;
            }
            // zera as variaveis
            usi = 0;
        }
      };
      // qsort the tmp_node
      qsort( tmp_node->lottery_ticket_arr, QTD_NUM_SORTEADOS, sizeof(tmp_node->lottery_ticket_arr[0]), qsort_asc );
        
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

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
 *   gcc -Wall -lconfig lf_filter.c -o lf_filter
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
 *   * 2019-05-15 - Uso de variavel static para parametros
 *                - Inclusao da SOMA na saida
 *                - Saida com combinacoes unicas quando se utiliza numeros randomicos
 *   * 2019-05-14 - Geracao de numeros randomicos
 *   * 2019-02-25 - Add "-r" option
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <libconfig.h>


#define NUM_MINIMO           1
#define NUM_MAXIMO           25
#define QTD_NUM_SORTEADOS    15
#define QTD_LINHAS           5
#define QTD_COLUNAS          5
#define NUM_MAX_FIXOS        QTD_NUM_SORTEADOS-1
#define NUM_MAX_DESCARTADOS  NUM_MAXIMO-QTD_NUM_SORTEADOS-1
#define MAX_LINE             64
#define CONF_FILE            "lf_filter.conf"


typedef struct {
  int numero; /* por questoes de compatibilidade com a libconfig, estou usando int
                 mas gostaria de estar usando unsigned short int */
  unsigned char dezenas_arr[QTD_NUM_SORTEADOS];
} concurso_t;

typedef struct node {
  unsigned char lottery_ticket_arr[QTD_NUM_SORTEADOS];
  struct node * next;
} node_t;


typedef struct {
  bool p6, p7, p8, p9,
       a7, a8, a9, a10,
       r4, r5, r6, r7,
       dl43332, dl44322, dl44331, dl53322, dl54321,
       dc43332, dc44322, dc44331, dc53322, dc54321;
  concurso_t ultimo_concurso;
  unsigned char arr_fixos[NUM_MAX_FIXOS+1], // +1 for "\0", to strlen
                arr_descartados[NUM_MAX_DESCARTADOS+1],
                seq_min_sorteados,
                seq_min_descartados;
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
      if ( arr_tmp[node->lottery_ticket_arr[i]] == 1 ) {
        seq_valida = false;
        break;
      }
      else {
        seq_valida = true;
        arr_tmp[node->lottery_ticket_arr[i]]++;
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


bool numeros_repetidos_concurso_anterior_ok( node_t *current, param_t *p ) {
  unsigned char uc1,
                uc2,
                num_repetidos = 0;
    
  for (uc1=0; uc1<QTD_NUM_SORTEADOS; uc1++)
    for (uc2=0; uc2<QTD_NUM_SORTEADOS; uc2++)
      if ( current->lottery_ticket_arr[uc1] == (p->ultimo_concurso).dezenas_arr[uc2] ) {
        num_repetidos++;
        break;
      }
    
  return ( ( ( p->a7 )  && ( num_repetidos == 7 ) ) ||
           ( ( p->a8 )  && ( num_repetidos == 8 ) ) ||
           ( ( p->a9 )  && ( num_repetidos == 9 ) ) ||
           ( ( p->a10 ) && ( num_repetidos == 10 ) ) );
}


bool numero_par_ok( node_t *current, param_t *p ) {
  unsigned char uc,
                even = 0;

  for (uc=0; uc<QTD_NUM_SORTEADOS; uc++)
    if ( current->lottery_ticket_arr[uc] % 2 == 0 )
      even++;

  return ( ( ( p->p6 ) && ( even == 6 ) ) ||
           ( ( p->p7 ) && ( even == 7 ) ) ||
           ( ( p->p8 ) && ( even == 8 ) ) ||
           ( ( p->p9 ) && ( even == 9 ) ) );
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
      } else {
        seq = 1; // reinicia a sequencia
      }
    }
    anterior = current->lottery_ticket_arr[uc];
  }
  
  return seq_ok;
}


bool seq_min_descartados_ok( node_t *current, unsigned char *seq_min_descartados ) {
  unsigned char uc,
                seq = 0; // contador da sequencia
  bool arr_check[NUM_MAXIMO] = {false};
  bool seq_ok = false;
                
  // inicializa o array com os numeros em suas respectivas posicoes
  // pois o calculo da sequencia dos descartados sera feito nesta base
  for (uc=0; uc<QTD_NUM_SORTEADOS; uc++)
    arr_check[current->lottery_ticket_arr[uc]-1] = current->lottery_ticket_arr[uc];

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

bool is_prime(int num) {
  if (num == 1)
    return false;
  if ( ((num % 2) == 0) && (num > 2) )
    return false;
  for (int i = 3; i < num / 2; i+= 2) {
    if ( (num % i) == 0 )
      return false;
  }
  return true;
}

bool qtd_primo_ok( node_t *current, param_t *p ) {
  unsigned char uc,
                primo = 0;

  for (uc=0; uc<QTD_NUM_SORTEADOS; uc++)
    if ( is_prime( current->lottery_ticket_arr[uc] ) )
      primo++;

  return ( ( ( p->r4 ) && ( primo == 4 ) ) ||
           ( ( p->r5 ) && ( primo == 5 ) ) ||
           ( ( p->r6 ) && ( primo == 6 ) ) ||
           ( ( p->r7 ) && ( primo == 7 ) ) );

}


bool distribuicao_linha_ok( node_t *current, param_t *p ) {
  unsigned short int padrao = 0;
  unsigned char uc,
                linha[QTD_LINHAS] = { 0 }; // A lotofacil possui 5 linhas: 1..5, 6..10, 11..15, 16..20 e 21..25

                
  for (uc=0; uc<QTD_NUM_SORTEADOS; uc++) {
    if ( ( current->lottery_ticket_arr[uc] < NUM_MINIMO ) || ( current->lottery_ticket_arr[uc] > NUM_MAXIMO ) ) {
      printf( "Invalid range in function 'distribuicao_linha_ok'.\n\n" );
      exit(1);
    }
    linha[(current->lottery_ticket_arr[uc]-1)/5]++; // armazena em grupos de 5 em 5, comecando pelo index 0. Dai o uso do -1.
  }

  qsort( linha, QTD_LINHAS, sizeof(linha[0]), qsort_desc );

  // Esta transformacao so funciona para array com inteiros de 1 digito E
  // o numero inteiro final precisa 'caber' no tipo da variavel padrao.
  // Neste caso, o valor maximo seria 5 (1 digito).
  for (uc = 0; uc < QTD_LINHAS; uc++)
    padrao = 10 * padrao + linha[uc];
  
  return ( ( ( p->dl43332 ) && ( padrao == 43332 ) ) ||
           ( ( p->dl44322 ) && ( padrao == 44322 ) ) ||
           ( ( p->dl44331 ) && ( padrao == 44331 ) ) ||
           ( ( p->dl53322 ) && ( padrao == 53322 ) ) ||
           ( ( p->dl54321 ) && ( padrao == 54321 ) ) );
}


bool distribuicao_coluna_ok( node_t *current, param_t *p ) {
  unsigned short int padrao = 0;
  unsigned char uc,
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

  for (uc=0; uc<QTD_NUM_SORTEADOS; uc++) {
    if ( ( current->lottery_ticket_arr[uc] < NUM_MINIMO ) || ( current->lottery_ticket_arr[uc] > NUM_MAXIMO ) ) {
      printf( "Invalid range in function 'distribuicao_coluna_ok'.\n\n" );
      exit(1);
    }
    last_digit = current->lottery_ticket_arr[uc] % 10;
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


void filter_list( node_t **head, param_t *p ) {
  node_t *current;

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

  if ( (p->a7) || (p->a8) || (p->a9) || (p->a10) ) {
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
  
  if ( strlen((char*)p->arr_fixos) > 0 ) {
    current = *head;
    while ( current != NULL ) {
      if ( filter_fixo( current, p->arr_fixos ) ) {
        pop_by_node( head, current );
        current = *head;
      } else {
        current = current->next;
      }
    }
  }

  if ( strlen((char*)p->arr_descartados) > 0 ) {
    current = *head;
    while ( current != NULL ) {
      if ( filter_descartado( current, p->arr_descartados ) ) {
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


void read_conf_file(concurso_t * ultimo_concurso) {
  unsigned char c, num;
  config_t cfg;               /* Returns all parameters in this structure */
  config_setting_t *setting;

  //char *config_file_name = CONF_FILE;

  /* Initialization */
  config_init(&cfg);

  // Read the file. If there is an error, report it and exit.
  //if (!config_read_file(&cfg, config_file_name))
  if (!config_read_file(&cfg, CONF_FILE))
  {
    printf("Erro ao abrir o arquivo %s!\n", CONF_FILE);
    printf("Detalhes: %s:%d - %s\n",
           config_error_file(&cfg),
           config_error_line(&cfg),
           config_error_text(&cfg));
    config_destroy(&cfg);
    exit(-1);
  }

  /* Read the "concursos" group */
  setting = config_lookup(&cfg, "ultimo_concurso");
  if (setting != NULL)
  {
        config_setting_t *dezenas = config_setting_get_member(setting, "dezenas");

        if (!(config_setting_lookup_int(setting, "numero", &(ultimo_concurso->numero))))
        {
            puts("ERRO! \"numero\" precisa ser definido!");
            config_destroy(&cfg);
            exit(-1);
        }

        if (!(dezenas))
        {
            puts("ERRO! \"dezenas\" precisa ser definido!");
            config_destroy(&cfg);
            exit(-1);
        }

        if ( config_setting_length(dezenas) != QTD_NUM_SORTEADOS )
        {
            printf("ERRO! %d dezenas precisam ser definidas!\n", QTD_NUM_SORTEADOS);
            config_destroy(&cfg);
            exit(-1);
        }
        
        for (c=0; c<QTD_NUM_SORTEADOS; c++) {
          num = config_setting_get_int_elem(dezenas, c);
          if (num < NUM_MINIMO || num > NUM_MAXIMO)
          {
            printf("ERRO! A dezena de posicao %d precisa ser definida entre %d e %d!\n", c+1, NUM_MINIMO, NUM_MAXIMO);
            config_destroy(&cfg);
            exit(-1);
          }
          else
            ultimo_concurso->dezenas_arr[c] = num;
        }
  }
  else
  {
    puts("Erro! Entrada \"ultimo_concurso\" nao encontrada!");
    config_destroy(&cfg);
    exit(-1);
  }

  config_destroy(&cfg);

  return;
}


void print_help( const char argv0[] ) {
  printf( "\n%s: Numero de parametros invalidos.\n\n", argv0 );
  printf( "Uso: %s [FILTRO] -n[numero combinacoes randomicas]|[arquivo matriz]\n\n", argv0 );
  printf( "Onde [FILTRO]:\n" );
  printf( "  -p <quantidade de pares>\n" );
  printf( "     Exemplo: -p7, filtra por 7 pares (e 8 impares)\n" );
  printf( "              -p8, filtra por 8 pares (e 7 impares)\n" );
  printf( "              -p6, filtra por 6 pares (e 9 impares)\n" );
  printf( "              -p9, filtra por 9 pares (e 6 impares)\n" );
  printf( "  -a<quantidade de numeros repetidos do concurso anterior>\n");
  printf( "     Estatisticamente:\n" );
  printf( "       -a9  = 33.50%%\n" );
  printf( "       -a8  = 25.07%%\n" );
  printf( "       -a10 = 21.02%%\n" );
  printf( "       -a7  =  9.27%%\n" );  
  printf( "  -i<min>, soma MINIMA de todos os numeros\n");
  printf( "  -x<max>, soma MAXIMA de todos os numeros\n");
  printf( "    * Estatisticamente, recomanda-se <min> = 184 e <max> = 199\n" );
  printf( "      O valor medio e 195.\n" );
  printf( "  -f<numero fixo>, fixa um numero nos jogos\n");
  printf( "    * O numero maximo de fixos e %d.\n", NUM_MAX_FIXOS );
  printf( "  -d<numero descartado>, descarta um numero nos jogos\n");
  printf( "    * O numero maximo de descartados e %d.\n", NUM_MAX_DESCARTADOS );
  printf( "  -l<distribuicao>, Distribuicao dos Numeros nas Linhas\n");
  printf( "    Possiveis valores para <distribuicao>:\n");
  printf( "      * 43332\n");
  printf( "      * 44322\n");
  printf( "      * 44331\n");
  printf( "      * 53322\n");
  printf( "      * 54321\n");
  printf( "  -c<distribuicao>, Distribuicao dos Numeros nas Colunas\n");
  printf( "    Possiveis valores para <distribuicao>:\n");
  printf( "      * 43332\n");
  printf( "      * 44322\n");
  printf( "      * 44331\n");
  printf( "      * 53322\n");
  printf( "      * 54321\n");
  printf( "  -s<seq minimo>, filtra por uma sequencia minima de numeros sorteados.\n");
  printf( "    * Exemplo: a combinacao ...02 04 06 07 08 09 13 14...\n" );
  printf( "      possui uma sequencia minima de 4 numeros sorteados  (06 07 08 e 09).\n" );
  printf( "      Neste caso, o parametro para filtro seria '-s4'.\n" );
  printf( "      Um bom parametro para o filtro seria '-s4' ou '-s5'.\n" );
  printf( "  -e<seq minimo>, filtra por uma sequencia minima de numeros nao sorteados.\n");
  printf( "    * Exemplo: a combinacao ...02 04 06 07 08 09 13 14...\n" );
  printf( "      possui uma sequencia minima de 3 numeros nao sorteados (10 11 e 12).\n" );
  printf( "      Neste caso, o parametro para filtro seria '-e3'.\n" );
  printf( "      Um bom parametro para o filtro seria '-e3' ou '-e4'.\n" );
  printf( "  -r<qtd primos>, filtra por uma quantidade de numeros primos no sorteio.\n");
  printf( "    * Obs: Possiveis valores para os parametros: -r4 -r5 -r6 -r7\n");
  printf( "      Possiveis numeros primos para LF: 2, 3, 5, 7, 11, 13, 17, 19, 23\n");
  printf( "      Estatisticamente, recomanda-se, em media, o uso de '-r5'\n\n" );
}

int main( int argc, char *argv[] ) {

  static param_t p;
  memset( &p, 0, sizeof(param_t) ); // clear full struct

  static node_t *head = NULL, /* Declare de head of the list. The head is used as a reference for all functions. */
                *tmp_node = NULL;

  unsigned char ln,
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
  
  while ( ( caractere = getopt ( argc, argv, "p:a:i:x:f:d:l:c:s:e:r:n:" ) ) != -1 ) {
    switch ( caractere ) {
      case 'p':
        switch ( atoi( optarg ) ) {
          case 6: p.p6 = true; break;
          case 7: p.p7 = true; break;
          case 8: p.p8 = true; break;
          case 9: p.p9 = true; break;
          default:
            printf( "\nERRO! Parametro de pares e impares '-p%s' invalido!\n\n", optarg );
            return 1;
        }
        break;
        
      case 'a':
        switch ( atoi( optarg ) ) {
          case  7: p.a7 = true; break;
          case  8: p.a8 = true; break;
          case  9: p.a9 = true; break;
          case 10: p.a10 = true; break;
          default:
            printf( "\nERRO! Parametro \"numero repetidos do concurso anterior\" '-a%s' invalido!\n\n", optarg );
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
          case 43332: p.dl43332 = true; break;
          case 44322: p.dl44322 = true; break;
          case 44331: p.dl44331 = true; break;
          case 53322: p.dl53322 = true; break;
          case 54321: p.dl54321 = true; break;
          default:
            printf( "\nERRO! Parametro de Distribuicao dos Numeros nas Linhas '-l%s' possui argumento invalido!\n\n", optarg );
            return 1;
        }
        break;
      case 'c':
        switch ( atoi( optarg ) ) {
          case 43332: p.dc43332 = true; break;
          case 44322: p.dc44322 = true; break;
          case 44331: p.dc44331 = true; break;
          case 53322: p.dc53322 = true; break;
          case 54321: p.dc54321 = true; break;
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
      case 'e':
        p.seq_min_descartados = atoi( optarg );
        if ( p.seq_min_descartados == 0 ) {
            printf( "\nERRO! Parametro de sequencia minima '-e' possui argumento invalido!\n\n" );
            return 1;
        }
        break;
      case 'r':
        switch ( atoi( optarg ) ) {
          case 4: p.r4 = true; break;
          case 5: p.r5 = true; break;
          case 6: p.r6 = true; break;
          case 7: p.r7 = true; break;
          default:
            printf( "\nERRO! Parametro de numero primo '-r%s' invalido!\n\n", optarg );
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
        if ( ( optopt == 'p' ) || ( optopt == 'a' ) || ( optopt == 'i' ) || ( optopt == 'x' ) || ( optopt == 'f' ) ||
             ( optopt == 'd' ) || ( optopt == 'l' ) || ( optopt == 'c' ) || ( optopt == 's' ) || ( optopt == 'e' ) ||
             ( optopt == 'r' ) || ( optopt == 'n' ) )
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
  }

  if (p.a7 || p.a8 || p.a9 || p.a10)
     read_conf_file(&(p.ultimo_concurso));

  tmp_node = create_node();

  if ( p.qtd_comb_randomicas > 0 ) {
    srand(time(0)); // inicializa a geracao de numeros randomicos com uma "semente"
    usi = 0; // contador de combinacoes randomicas validas
    
    // faz o filtro pela quatidade de combinacoes randomicas geradas pelos sistema,
    // ignorando o arquito matriz
    
    while ( usi < p.qtd_comb_randomicas ) {
      // Gera uma sequencia randomica valida
      get_valid_sequence( tmp_node );
      
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
          printf( "\nERRO! Numero '%d' fora do escopo da Lotofacil!\n\n", n );
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

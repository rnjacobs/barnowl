#include <string.h>
#include "owl.h"

static const char fileIdent[] = "$Id$";

int owl_filter_init_fromstring(owl_filter *f, char *name, char *string)
{
  char **argv;
  int argc, out;

  argv=owl_parseline(string, &argc);
  out=owl_filter_init(f, name, argc, argv);
  /* owl_parsefree(argv, argc); */
  return(out);
}

int owl_filter_init(owl_filter *f, char *name, int argc, char **argv)
{
  int i, error;
  owl_filterelement *fe;
  char *regexstr;
   
  f->name=owl_strdup(name);
  f->polarity=0;
  f->color=OWL_COLOR_DEFAULT;
  f->cachedmsgid=-1;
  owl_list_create(&(f->fes));
  
  /* first take arguments that have to come first */
  /* set the color */
  if (argc>=2 && !strcmp(argv[0], "-c")) {
    f->color=owl_util_string_to_color(argv[1]);
    argc-=2;
    argv+=2;
  }
  
  /* then deal with the expression */
  for (i=0; i<argc; i++) {
    error=0;
    fe=owl_malloc(sizeof(owl_filterelement));
    
    /* all the 0 argument possibilities */
    if (!strcmp(argv[i], "(")) {
      owl_filterelement_create_openbrace(fe);
    } else if (!strcmp(argv[i], ")")) {
      owl_filterelement_create_closebrace(fe);
    } else if (!strcasecmp(argv[i], "and")) {
      owl_filterelement_create_and(fe);
    } else if (!strcasecmp(argv[i], "or")) {
      owl_filterelement_create_or(fe);
    } else if (!strcasecmp(argv[i], "not")) {
      owl_filterelement_create_not(fe);
    } else if (!strcasecmp(argv[i], "true")) {
      owl_filterelement_create_true(fe);
    } else if (!strcasecmp(argv[i], "false")) {
      owl_filterelement_create_false(fe);
      
    } else if (i==argc-1) {
      error=1;
    } else {
      if (!strcasecmp(argv[i], "class") ||
	  !strcasecmp(argv[i], "instance") ||
	  !strcasecmp(argv[i], "sender") ||
	  !strcasecmp(argv[i], "recipient") ||
	  !strcasecmp(argv[i], "body") ||
	  !strcasecmp(argv[i], "opcode") ||
	  !strcasecmp(argv[i], "realm") ||
	  !strcasecmp(argv[i], "type") ||
	  !strcasecmp(argv[i], "direction") ||
	  !strcasecmp(argv[i], "login")) {
	regexstr=owl_util_substitute(argv[i+1], "%me%", ZGetSender());
	owl_filterelement_create_re(fe, argv[i], regexstr);
	owl_free(regexstr);
	i++;
      } else {
	error=1;
      }
    }

    if (!error) {
      owl_list_append_element(&(f->fes), fe);
    } else {
      owl_free(fe);
      owl_filter_free(f);
      return(-1);
    }

  }
  return(0);
}

char *owl_filter_get_name(owl_filter *f)
{
  return(f->name);
}

void owl_filter_set_polarity_match(owl_filter *f)
{
  f->polarity=0;
}

void owl_filter_set_polarity_unmatch(owl_filter *f)
{
  f->polarity=1;
}

void owl_filter_set_color(owl_filter *f, int color)
{
  f->color=color;
}

int owl_filter_get_color(owl_filter *f)
{
  return(f->color);
}

void owl_filter_set_cachedmsgid(owl_filter *f, int cachedmsgid)
{
  f->cachedmsgid=cachedmsgid;
}

int owl_filter_get_cachedmsgid(owl_filter *f)
{
  return(f->cachedmsgid);
}

int owl_filter_message_match(owl_filter *f, owl_message *m)
{
  int i, j, tmp;
  owl_list work_fes, *fes;
  owl_filterelement *fe;
  char *field, *match;

  /* create the working list of expression elements */
  fes=&(f->fes);
  owl_list_create(&work_fes);
  j=owl_list_get_size(fes);
  for (i=0; i<j; i++) {
    owl_list_append_element(&work_fes, owl_list_get_element(fes, i));
  }

  /* first go thru and evaluate all RE elements to true or false */
  match="";
  for (i=0; i<j; i++) {
    fe=owl_list_get_element(&work_fes, i);
    if (!owl_filterelement_is_re(fe)) continue;
    field=owl_filterelement_get_field(fe);
    if (!strcasecmp(field, "class")) {
      match=owl_message_get_class(m);
    } else if (!strcasecmp(field, "instance")) {
      match=owl_message_get_instance(m);
    } else if (!strcasecmp(field, "sender")) {
      match=owl_message_get_sender(m);
    } else if (!strcasecmp(field, "recipient")) {
      match=owl_message_get_recipient(m);
    } else if (!strcasecmp(field, "body")) {
      match=owl_message_get_body(m);
    } else if (!strcasecmp(field, "opcode")) {
      match=owl_message_get_opcode(m);
    } else if (!strcasecmp(field, "realm")) {
      match=owl_message_get_realm(m);
    } else if (!strcasecmp(field, "type")) {
      match=owl_message_type_to_string(m);
    } else if (!strcasecmp(field, "direction")) {
      if (owl_message_is_direction_out(m)) {
	match="out";
      } else if (owl_message_is_direction_in(m)) {
	match="in";
      } else if (owl_message_is_direction_none(m)) {
	match="none";
      } else {
	match="";
      }
    } else if (!strcasecmp(field, "login")) {
      if (owl_message_is_login(m)) {
	match="login";
      } else if (owl_message_is_logout(m)) {
	match="logout";
      } else {
	match="none";
      }
    }
    
    tmp=owl_regex_compare(owl_filterelement_get_re(fe), match);
    if (!tmp) {
      owl_list_replace_element(&work_fes, i, owl_global_get_filterelement_true(&g));
    } else {
      owl_list_replace_element(&work_fes, i, owl_global_get_filterelement_false(&g));
    }
  }

  /* call the recrsive helper */
  i=_owl_filter_message_match_recurse(f, m, &work_fes, 0, owl_list_get_size(&(f->fes))-1);

  /* now there will be only one TRUE / FALSE, find it among the NULL's */
  tmp=0;
  for (i=0; i<j; i++) {
    fe=owl_list_get_element(&work_fes, i);
    if (owl_filterelement_is_null(fe)) continue;
    if (owl_filterelement_is_true(fe)) {
      tmp=1;
      break;
    }
    if (owl_filterelement_is_false(fe)) {
      tmp=0;
      break;
    }
  }  

  /* reverse the answer if negative polarity is in use */
  if (f->polarity) tmp=!tmp;

  owl_list_free_simple(&work_fes);
  return(tmp);
}

int _owl_filter_message_match_recurse(owl_filter *f, owl_message *m, owl_list *fes, int start, int end)
{
  int a=0, b=0, i, x, y, z, score, ret, type;
  owl_filterelement *fe, *tmpfe=NULL;

  /* Deal with parens first. */
  for (i=0; i<OWL_FILTER_MAX_DEPTH; i++) {
    /* Find first open paren and matching close paren, store in x, y */
    score=x=y=0;
    for (i=start; i<=end; i++) {
      fe=owl_list_get_element(fes, i);
      if (owl_filterelement_is_openbrace(fe)) {
	if (score==0) x=i;
	score++;
      } else if (owl_filterelement_is_closebrace(fe)) {
	score--;
	if (score<0) {
	  /* unblanaced parens */
	  return(-1);
	} else if (score==0) {
	  y=i; /* this is the matching close paren */
	  break;
	}
      }
    }
    if (score>0) {
      /* unblanaced parens */
      return(-1);
    }

    /* Simply the parens by removing them and evaluating what was in between */
    if (y>0) {
      /* null out the parens */
      owl_list_replace_element(fes, x, owl_global_get_filterelement_null(&g));
      owl_list_replace_element(fes, y, owl_global_get_filterelement_null(&g));

      /* evaluate expression in between */
      ret=_owl_filter_message_match_recurse(f, m, fes, x+1, y-1);
      if (ret<0) return(-1);

      /* there may be more, so we continue */
      continue;
    } else {
      /* otherwise we're done with this part */
      break;
    }
  }
  if (i==OWL_FILTER_MAX_DEPTH) {
    /* hit the saftey limit, consider it invalid */
    return(-1);
  }

  /* Find AND / OR / NOT.
   *   For binary expressions (AND/OR):
   *     "type" is 1
   *     "x" will index first val, "y" the operator and "z" the second val
   *   For unary expressions (NOT):
   *     "type" is 2
   *     "x" will index the operator, "y" the value
   *   "score" tallys how many expression elements have been found so far
   */
  for (i=0; i<OWL_FILTER_MAX_DEPTH; i++) {
    type=score=x=y=z=0;
    for (i=start; i<=end; i++) {
      fe=owl_list_get_element(fes, i);
      if (owl_filterelement_is_null(fe)) continue;
      if (score==0) {
	if (owl_filterelement_is_value(fe)) {
	  x=i;
	  score=1;
	  type=1;
	} else if (owl_filterelement_is_not(fe)) {
	  x=i;
	  score=1;
	  type=2;
	}
      } else if (score==1) {
	if (type==1) {
	  if (owl_filterelement_is_and(fe) || owl_filterelement_is_or(fe)) {
	    score=2;
	    y=i;
	  } else {
	    /* it's not a valid binary expression */
	    x=y=z=score=0;
	  }
	} else if (type==2) {
	  if (owl_filterelement_is_value(fe)) {
	    /* valid unary expression, we're done */
	    y=i;
	    break;
	  }
	}
      } else if (score==2) {
	if (owl_filterelement_is_value(fe)) {
	  /* valid binary expression, we're done */
	  z=i;
	  break;
	} else {
	  x=y=z=score=0;
	}
      }
    }

    /* simplify AND / OR */
    if ((type==1) && (z>0)) {
      fe=owl_list_get_element(fes, x);
      if (owl_filterelement_is_true(fe)) {
	a=1;
      } else if (owl_filterelement_is_false(fe)) {
	a=0;
      }

      fe=owl_list_get_element(fes, z);
      if (owl_filterelement_is_true(fe)) {
	b=1;
      } else if (owl_filterelement_is_false(fe)) {
	b=0;
      }

      fe=owl_list_get_element(fes, y);
      if (owl_filterelement_is_and(fe)) {
	if (a && b) {
	  tmpfe=owl_global_get_filterelement_true(&g);
	} else {
	  tmpfe=owl_global_get_filterelement_false(&g);
	}
      } else if (owl_filterelement_is_or(fe)) {
	if (a || b) {
	  tmpfe=owl_global_get_filterelement_true(&g);
	} else {
	  tmpfe=owl_global_get_filterelement_false(&g);
	}
      }
      owl_list_replace_element(fes, x, owl_global_get_filterelement_null(&g));
      owl_list_replace_element(fes, y, tmpfe);
      owl_list_replace_element(fes, z, owl_global_get_filterelement_null(&g));
    } else if ((type==2) && (y>0)) {
      /* simplify NOT */
      fe=owl_list_get_element(fes, y);
      owl_list_replace_element(fes, x, owl_global_get_filterelement_null(&g));
      if (owl_filterelement_is_false(fe)) {
	owl_list_replace_element(fes, y, owl_global_get_filterelement_true(&g));
      } else {
	owl_list_replace_element(fes, y, owl_global_get_filterelement_false(&g));
      }
    } else {
      break;
    }
  }
  return(0);

}

void owl_filter_print(owl_filter *f, char *out)
{
  int i, j;
  owl_filterelement *fe;
  char *tmp;

  strcpy(out, owl_filter_get_name(f));
  strcat(out, ": ");

  if (f->color!=OWL_COLOR_DEFAULT) {
    strcat(out, "-c ");
    strcat(out, owl_util_color_to_string(f->color));
    strcat(out, " ");
  }

  j=owl_list_get_size(&(f->fes));
  for (i=0; i<j; i++) {
    fe=owl_list_get_element(&(f->fes), i);
    tmp=owl_filterelement_to_string(fe);
    strcat(out, tmp);
    owl_free(tmp);
  }
  strcat(out, "\n");
}

int owl_filter_equiv(owl_filter *a, owl_filter *b)
{
  char buff[LINE], buff2[LINE];

  owl_filter_print(a, buff);
  owl_filter_print(b, buff2);

  if (!strcmp(buff, buff2)) return(1);
  return(0);
}

void owl_filter_free(owl_filter *f)
{
  void (*func)();

  func=&owl_filterelement_free;
  
  if (f->name) owl_free(f->name);
  owl_list_free_all(&(f->fes), func);
}

#include <nginx.h>
#include <ngx_core.h>
#include <ngx_config.h>
#include <ngx_http.h>

static ngx_int_t ngx_http_auth_token_init (ngx_conf_t*);

ngx_http_module_t ngx_http_auth_token_module_ctx[] = {
    0,                                    /* preconfiguration */
    ngx_http_auth_token_init,             /* postconfiguration */
    0,                                    /* create_main_conf */
    0,                                    /* init main conf    */
    0,                                    /* create srv conf */
    0,                                    /* merge srv conf */
    0,                                    /* create loc conf */
    0                                     /* merge loc conf */
};

ngx_module_t ngx_http_auth_token_module[] = {
    NGX_MODULE_V1,                      /* Header of the struct */
    &ngx_http_auth_token_module_ctx,    /* Module context       */
    0,                                  /* Module directives    */
    NGX_HTTP_MODULE,                    /* Module type          */
    0,                                  /* Init master          */
    0,                                  /* Init module          */
    0,                                  /* Init process         */
    0,                                  /* Init thread          */
    0,                                  /* Exit thread          */
    0,                                  /* Exit process         */
    0,                                  /* Exit master          */
    NGX_MODULE_V1_PADDING               /* Footer of the struct */
};

static ngx_int_t
ngx_http_auth_token_handler(ngx_http_request_t *request)
{
    /* If request has alrady been processed, return in order to allow nginx to proceed with the subsequent phase of processing */
    if (request->main->internal)
    {
        return NGX_DECLINED;
    }

    /* Mark this request as having been already processed */
    request->main->internal = 1;

    /* Allocates and add a new header to the request's headers list, then assign it to nginx hash table */
    ngx_table_elt_t *headerElt = ngx_list_push(&request->headers_out.headers);

    /* Populate header element with relevant values */
    headerElt->hash = 1; // Set to 1 in the nginx hash table to indicate to nginx that the element is to be used
    ngx_str_set(&headerElt->key, "X-NGINX_Tutorial");
    ngx_str_set(&headerElt->value, "Hello NGINX world !");

    return NGX_DECLINED;
}

static ngx_int_t
ngx_http_auth_token_init(ngx_conf_t *cf)
{
    ngx_http_core_main_conf_t *cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    ngx_http_handler_pt *handler = ngx_array_push(&cmcf->phases[NGX_HTTP_ACCESS_PHASE].handlers);
    if (!handler)
    {
        return NGX_ERROR;
    }

    *handler = ngx_http_auth_token_handler;

    return NGX_OK;
}
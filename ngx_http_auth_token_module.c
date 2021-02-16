#include <stdlib.h>
#include <nginx.h>
#include <ngx_core.h>
#include <ngx_config.h>
#include <ngx_http.h>
#include <string.h>
#include <hiredis/hiredis.h>

#define REDIS_IP "localhost"
#define REDIS_PORT 6379

static ngx_int_t ngx_http_auth_token_init(ngx_conf_t *);

static ngx_http_module_t ngx_http_auth_token_module_ctx = {
    0,                        /* preconfiguration */
    ngx_http_auth_token_init, /* postconfiguration */
    0,                        /* create_main_conf */
    0,                        /* init main conf    */
    0,                        /* create srv conf */
    0,                        /* merge srv conf */
    0,                        /* create loc conf */
    0                         /* merge loc conf */
};

ngx_module_t ngx_http_auth_token_module = {
    NGX_MODULE_V1,                   /* Header of the struct */
    &ngx_http_auth_token_module_ctx, /* Module context       */
    0,                               /* Module directives    */
    NGX_HTTP_MODULE,                 /* Module type          */
    0,                               /* Init master          */
    0,                               /* Init module          */
    0,                               /* Init process         */
    0,                               /* Init thread          */
    0,                               /* Exit thread          */
    0,                               /* Exit process         */
    0,                               /* Exit master          */
    NGX_MODULE_V1_PADDING            /* Footer of the struct */
};

static ngx_int_t 
lookup_user(ngx_str_t *user_id, ngx_str_t *auth_token)
{
    redisContext *redisCtx = redisConnect(REDIS_IP, REDIS_PORT);
    if (!redisCtx || redisCtx->err)
    {
        if (redisCtx)
            ngx_log_stderr(0, "Redis error: %s\n", redisCtx->errstr);
        else
            ngx_log_stderr(0, "Redis Context allocation failed\n");

        return NGX_DECLINED;
    }

    redisReply *redisReply = redisCommand(redisCtx, "GET %s", auth_token->data);
    if (REDIS_REPLY_NIL == redisReply->type)
    {
        ngx_log_stderr(0, "Redis reply error: null reply");
        return NGX_DECLINED;
    }
    else
    {
        ngx_str_set(user_id, redisReply->str);
    }

    freeReplyObject(redisReply);
    redisFree(redisCtx);
}

static ngx_int_t
ngx_http_auth_token_handler(ngx_http_request_t *request)
{
    ngx_log_stderr(0, "Entering %s", __FUNCTION__);

    /* If request has alrady been processed, return in order to allow nginx to 
    proceed with the subsequent phase of processing */
    if (request->main->internal)
    {
        return NGX_DECLINED;
    }

    /* Mark this request as having been already processed */
    request->main->internal = 1;

    ngx_str_t cookie = ngx_string("auth-token");
    ngx_str_t cookie_value;
    ngx_int_t cookieIndex = ngx_http_parse_multi_header_lines(&request->headers_in.cookies,
                                                              &cookie,
                                                              &cookie_value);

    ngx_log_stderr(0, "Just parsed request headers for cookie");

    /* Allocates and add a new header to the request's response's headers list, 
    then assign it to nginx hash table */
    ngx_table_elt_t *headerElt = ngx_list_push(&request->headers_out.headers);

    headerElt->hash = 1; // Set to 1 in the nginx hash table to indicate to nginx that the element is to be used

    if (NGX_DECLINED == cookieIndex)
    {
        ngx_log_stderr(0, "cookie header not found");
        ngx_str_set(&headerElt->key, "Location");
        ngx_str_set(&headerElt->value, "http://google.com");
        return NGX_HTTP_MOVED_PERMANENTLY;
    }
    else
    {
        ngx_log_stderr(0, "cookie found, sending it back in the response ...");
        headerElt->key = cookie;
        headerElt->value = cookie_value;
    }

    ngx_log_stderr(0, "Exiting %s", __FUNCTION__);

    return NGX_DECLINED;
}

static ngx_int_t
ngx_http_auth_token_init(ngx_conf_t *cf)
{
    ngx_log_stderr(0, "Entering %s", __FUNCTION__);

    ngx_http_core_main_conf_t *cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    ngx_http_handler_pt *handler = ngx_array_push(&cmcf->phases[NGX_HTTP_ACCESS_PHASE].handlers);
    if (!handler)
    {
        return NGX_ERROR;
    }

    *handler = ngx_http_auth_token_handler;

    ngx_log_stderr(0, "Exiting %s", __FUNCTION__);

    return NGX_OK;
}
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
lookup_user(ngx_str_t *userId, ngx_str_t *authToken, ngx_http_request_t *request)
{
    ngx_log_error(NGX_LOG_DEBUG, request->connection->log, 0,
                  "==> Entering %s\n", __FUNCTION__);

    redisContext *redisCtx = redisConnect(REDIS_IP, REDIS_PORT);
    if (!redisCtx || redisCtx->err)
    {
        if (redisCtx)
            ngx_log_error(NGX_LOG_ERR, request->connection->log, 0,
                          "Redis error: %s\n", redisCtx->errstr);
        else
            ngx_log_error(NGX_LOG_ERR, request->connection->log, 0,
                          "Redis Context allocation failed\n");

        ngx_log_error(NGX_LOG_ERR, request->connection->log, 0,
                      "About to return error\n");
        return NGX_DECLINED;
    }

    ngx_log_error(NGX_LOG_DEBUG, request->connection->log, 0,
                  "Querying Redis backend DB for user id associated with provided authToken");
    redisReply *redisReply = redisCommand(redisCtx, "GET %s", authToken->data);

    ngx_log_error(NGX_LOG_DEBUG, request->connection->log, 0,
                  "Redis server DB queried. Checking result ...\n");

    if (REDIS_REPLY_NIL == redisReply->type)
    {
        ngx_log_error(NGX_LOG_ERR, request->connection->log, 0,
                      "Redis reply error: null reply (redisReply->len=%d)\n", redisReply->len);
        return NGX_DECLINED;
    }

    // Make deep copy, to subsequently free redis string without impacting the destination string
    userId->len = redisReply->len;
    userId->data = ngx_palloc(request->pool, userId->len);
    ngx_memcpy(userId->data, redisReply->str, userId->len); // ngx_cpystrn requires len+1 to copy len character and terminate destination with null terminator

    freeReplyObject(redisReply);
    redisFree(redisCtx);

    ngx_log_error(NGX_LOG_DEBUG, request->connection->log, 0,
                  "<== Exiting %s\n", __FUNCTION__);

    return NGX_OK;
}

static ngx_int_t
redirect(ngx_table_elt_t *outHeaderElt, ngx_log_t *log)
{
    ngx_log_error(NGX_LOG_DEBUG, log, 0,
                  "==> Entering %s\n", __FUNCTION__);

    ngx_str_set(&outHeaderElt->key, "Location");
    ngx_str_set(&outHeaderElt->value, "http://google.com");

    ngx_log_error(NGX_LOG_DEBUG, log, 0,
                  "<== Exiting %s\n", __FUNCTION__);

    return NGX_HTTP_MOVED_PERMANENTLY;
}

static void
appendOutHeaderElt(ngx_table_elt_t *outHeaderElt,
                   ngx_str_t *outHeaderKey,
                   ngx_str_t *outHeaderValue,
                   ngx_log_t *log)
{
    ngx_log_error(NGX_LOG_DEBUG, log, 0,
                  "==> Entering %s\n", __FUNCTION__);

    outHeaderElt->key   = *outHeaderKey;
    outHeaderElt->value = *outHeaderValue;

    ngx_log_error(NGX_LOG_DEBUG, log, 0,
                  "Appended this header to the response's outgoing headers list :\n"
                  "\toutHeaderElt->key->len  = %d\n"
                  "\toutHeaderElt->key->data = %s\n",
                  outHeaderElt->key.len, outHeaderElt->key.data);

    ngx_log_error(NGX_LOG_DEBUG, log, 0,
                  "<== Exiting %s\n", __FUNCTION__);
}

static ngx_int_t
ngx_http_auth_token_handler(ngx_http_request_t *request)
{
    ngx_log_error(NGX_LOG_DEBUG, request->connection->log, 0,
                  "==> Entering %s\n", __FUNCTION__);

    /* If request has alrady been processed, return in order to allow nginx to 
    proceed with the subsequent phase of processing */
    if (request->main->internal)
    {
        ngx_log_error(NGX_LOG_ERR, request->connection->log, 0,
                      "Request already processed. Aborting.\n");
        return NGX_DECLINED;
    }

    /* Mark this request as having been already processed */
    request->main->internal = 1;

    ngx_str_t authTokenCookie = ngx_string("auth-token");
    ngx_str_t authTokenCookieValue;
    ngx_int_t authTokenCookieIndex = ngx_http_parse_multi_header_lines(&request->headers_in.cookies,
                                                                       &authTokenCookie,
                                                                       &authTokenCookieValue);

    ngx_log_error(NGX_LOG_DEBUG, request->connection->log, 0,
                  "Just parsed request headers for cookie\n");

    /* Allocates and add a new header to the request's response's headers list, 
    then assign it to nginx hash table */
    ngx_table_elt_t *outHeaderElt = ngx_list_push(&request->headers_out.headers);

    // Set to 1 in the nginx hash table to indicate to nginx that the element
    // is to be used
    outHeaderElt->hash = 1;

    if (NGX_DECLINED == authTokenCookieIndex) // Cookie not found
    {
        ngx_log_error(NGX_LOG_ERR, request->connection->log, 0,
                      "cookie header %s not found in request's incoming headers\n",
                      authTokenCookie.data);
        return redirect(outHeaderElt, request->connection->log);
    }

    ngx_str_t userId;

    // User corresponding to authTokenCookie not found in Redis backend DB
    if (NGX_DECLINED == lookup_user(&userId, &authTokenCookieValue, request))
    {
        ngx_log_error(NGX_LOG_DEBUG, request->connection->log, 0,
                      "user corresponding to auth cookie %s not found in Redis backend DB\n",
                      authTokenCookieValue.data);
        return redirect(outHeaderElt, request->connection->log);
    }

    appendOutHeaderElt(outHeaderElt,
                       &(ngx_str_t)ngx_string("User-ID"),
                       &userId,
                       request->connection->log);

    ngx_log_error(NGX_LOG_DEBUG, request->connection->log, 0,
                  "<== Exiting %s\n", __FUNCTION__);

    return NGX_DECLINED;
}

static ngx_int_t
ngx_http_auth_token_init(ngx_conf_t *cf)
{
    ngx_log_error(NGX_LOG_DEBUG, cf->log, 0, "==> Entering %s\n", __FUNCTION__);

    ngx_http_core_main_conf_t *cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    ngx_http_handler_pt *handler = ngx_array_push(&cmcf->phases[NGX_HTTP_ACCESS_PHASE].handlers);
    if (!handler)
    {
        ngx_log_error(NGX_LOG_ERR, cf->log, 0,
                      "NGX_HTTP_ACCESS_PHASE's new handler's allocation failed. Abort\n");
        return NGX_ERROR;
    }

    *handler = ngx_http_auth_token_handler;

    ngx_log_error(NGX_LOG_DEBUG, cf->log, 0, "<== Exiting %s\n", __FUNCTION__);

    return NGX_OK;
}

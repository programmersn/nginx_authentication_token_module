#include <stdlib.h>
#include <nginx.h>
#include <ngx_core.h>
#include <ngx_config.h>
#include <ngx_http.h>
#include <string.h>
#include <hiredis/hiredis.h>

static ngx_int_t ngx_http_auth_token_init(ngx_conf_t *);
static void *ngx_http_auth_token_create_main_conf(ngx_conf_t *);
typedef struct auth_token_main_conf_s
{
    ngx_int_t redis_port;
    ngx_str_t redis_host;
    ngx_str_t cookie_name;
    ngx_str_t redirect_location;
} auth_token_main_conf_t;

static ngx_command_t ngx_http_auth_token_commands[] = {
    {ngx_string("auth_token_redis_port"),
     NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
     ngx_conf_set_num_slot,
     NGX_HTTP_MAIN_CONF_OFFSET,
     offsetof(auth_token_main_conf_t, redis_port),
     0},
    {ngx_string("auth_token_redis_host"),
     NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
     ngx_conf_set_str_slot,
     NGX_HTTP_MAIN_CONF_OFFSET,
     offsetof(auth_token_main_conf_t, redis_host),
     0},
    {ngx_string("auth_token_cookie_name"),
     NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
     ngx_conf_set_str_slot,
     NGX_HTTP_MAIN_CONF_OFFSET,
     offsetof(auth_token_main_conf_t, cookie_name),
     0},
    {ngx_string("auth_token_redirect_location"),
     NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
     ngx_conf_set_str_slot,
     NGX_HTTP_MAIN_CONF_OFFSET,
     offsetof(auth_token_main_conf_t, redirect_location),
     0},
    ngx_null_command // standard doc use ngx_null_command instead
};

static ngx_http_module_t ngx_http_auth_token_module_ctx = {
    0,                                    /* preconfiguration */
    ngx_http_auth_token_init,             /* postconfiguration */
    ngx_http_auth_token_create_main_conf, /* create_main_conf */
    0,                                    /* init main conf    */
    0,                                    /* create srv conf */
    0,                                    /* merge srv conf */
    0,                                    /* create loc conf */
    0                                     /* merge loc conf */
};

ngx_module_t ngx_http_auth_token_module = {
    NGX_MODULE_V1,                   /* Header of the struct */
    &ngx_http_auth_token_module_ctx, /* Module context       */
    ngx_http_auth_token_commands,    /* Module directives    */
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
lookup_user(ngx_str_t *userId, ngx_str_t *authToken, ngx_http_request_t *request,
            auth_token_main_conf_t *mainConf)
{
    ngx_log_error(NGX_LOG_DEBUG, request->connection->log, 0,
                  "==> Entering %s\n", __FUNCTION__);

    redisContext *redisCtx = redisConnect(mainConf->redis_host.data, mainConf->redis_port);
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
    userId->data = ngx_palloc(request->pool, userId->len);  // TODO: add check on returned pointer
    ngx_memcpy(userId->data, redisReply->str, userId->len); // ngx_cpystrn requires len+1 to copy len character and terminate destination with null terminator

    // Free redis allocated objects
    freeReplyObject(redisReply);
    redisFree(redisCtx);

    ngx_log_error(NGX_LOG_DEBUG, request->connection->log, 0,
                  "<== Exiting %s\n", __FUNCTION__);

    return NGX_OK;
}

static ngx_int_t
redirect(ngx_table_elt_t *authTokenHeaderElt, ngx_log_t *log, auth_token_main_conf_t *mainConf)
{
    ngx_log_error(NGX_LOG_DEBUG, log, 0,
                  "==> Entering %s\n", __FUNCTION__);

    ngx_str_set(&authTokenHeaderElt->key, "Location");
    authTokenHeaderElt->value = mainConf->redirect_location;

    ngx_log_error(NGX_LOG_DEBUG, log, 0, "<== Exiting %s\n", __FUNCTION__);

    return NGX_HTTP_MOVED_PERMANENTLY;
}

static void
initHeaderElt(ngx_table_elt_t *authTokenHeaderElt,
              ngx_str_t *headerKey,
              ngx_str_t *headerValue,
              ngx_log_t *log)
{
    ngx_log_error(NGX_LOG_DEBUG, log, 0,
                  "==> Entering %s\n", __FUNCTION__);

    authTokenHeaderElt->key = *headerKey;
    authTokenHeaderElt->value = *headerValue;

    ngx_log_error(NGX_LOG_DEBUG, log, 0,
                  "Initilization of following header done :\n"
                  "\toutHeaderElt->key->len  = %d\n"
                  "\toutHeaderElt->key->data = %s\n",
                  authTokenHeaderElt->key.len, authTokenHeaderElt->key.data);

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

    ngx_log_error(NGX_LOG_DEBUG, request->connection->log, 0,
                  "Getting pointer to main conf object");

    auth_token_main_conf_t *mainConf = ngx_http_get_module_main_conf(request, ngx_http_auth_token_module);

    ngx_str_t authTokenCookie = mainConf->cookie_name;
    ngx_str_t authTokenCookieValue;
    ngx_int_t authTokenCookieIndex = ngx_http_parse_multi_header_lines(&request->headers_in.cookies,
                                                                       &authTokenCookie,
                                                                       &authTokenCookieValue);

    ngx_log_error(NGX_LOG_DEBUG, request->connection->log, 0,
                  "Just parsed request headers for cookie\n");

    /* Allocates and add a new header to the request's headers list, then assign it to nginx hash table */
    ngx_table_elt_t *authTokenHeaderElt = ngx_list_push(&request->headers_in.headers);

    // Set to 1 in the nginx hash table to indicate to nginx that the element is to be used
    authTokenHeaderElt->hash = 1;

    if (NGX_DECLINED == authTokenCookieIndex) // Cookie not found
    {
        ngx_log_error(NGX_LOG_ERR, request->connection->log, 0,
                      "cookie header %s not found in request's incoming headers\n",
                      authTokenCookie.data);
        return redirect(authTokenHeaderElt, request->connection->log, mainConf);
    }

    ngx_str_t userId;

    // User corresponding to authTokenCookie not found in Redis backend DB
    if (NGX_DECLINED == lookup_user(&userId, &authTokenCookieValue, request, mainConf))
    {
        ngx_log_error(NGX_LOG_DEBUG, request->connection->log, 0,
                      "user corresponding to auth cookie %s not found in Redis backend DB\n",
                      authTokenCookieValue.data);
        return redirect(authTokenHeaderElt, request->connection->log, mainConf);
    }

    initHeaderElt(authTokenHeaderElt,
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
    ngx_log_stderr(NGX_LOG_DEBUG, "==> Entering %s\n", __FUNCTION__);

    ngx_http_core_main_conf_t *cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    ngx_http_handler_pt *handler = ngx_array_push(&cmcf->phases[NGX_HTTP_ACCESS_PHASE].handlers);
    if (!handler)
    {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0,
                           "NGX_HTTP_ACCESS_PHASE's new handler's allocation failed. Abort\n");
        return NGX_ERROR;
    }

    *handler = ngx_http_auth_token_handler;

    ngx_log_stderr(NGX_LOG_DEBUG, "<== Exiting %s\n", __FUNCTION__);

    return NGX_OK;
}

static void *
ngx_http_auth_token_create_main_conf(ngx_conf_t *cf)
{
    ngx_log_stderr(NGX_LOG_DEBUG, "==> Entering %s", __FUNCTION__);

    auth_token_main_conf_t *mainConf = ngx_pcalloc(cf->pool, sizeof *mainConf);
    if (!mainConf)
    {
        ngx_log_stderr(NGX_LOG_DEBUG,
                       "Couldn't allocate memory from pool. Abort.\n");
        return 0;
    }

    mainConf->redis_port = NGX_CONF_UNSET_UINT;

    ngx_log_stderr(NGX_LOG_DEBUG, "<== Exiting %s", __FUNCTION__);

    return mainConf;
}
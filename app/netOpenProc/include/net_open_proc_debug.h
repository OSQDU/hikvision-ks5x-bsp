/** 
* @file net_open_proc_debug.h
*
* @note V1.0 initial
*/

#ifndef __NET_OPEN_PROC_DEBUG_H__
#define __NET_OPEN_PROC_DEBUG_H__

#define NET_OPEN_PROC_DEBUG 
#ifdef NET_OPEN_PROC_DEBUG

#define NET_OPEN_PROC_INFO(...)         do{\
                                            fprintf(stderr, "file:%s,line:%d, ",__func__,__LINE__);\
                                            fprintf(stderr, __VA_ARGS__);\
                                        }while(0) 
                                        
#define NET_OPEN_PROC_ERR(...)          do{\
                                            fprintf(stderr, "file:%s,line:%d, ",__func__,__LINE__);\
                                            fprintf(stderr, __VA_ARGS__);\
                                        }while(0)
                                        
#define NET_OPEN_PROC_FATAL(...)        do{\
                                            fprintf(stderr, "file:%s,line:%d, ",__func__,__LINE__);\
                                            fprintf(stderr, __VA_ARGS__);\
                                        }while(0) 
#else
#define NET_OPEN_PROC_INFO(...)         
#define NET_OPEN_PROC_ERR(...)          
#define NET_OPEN_PROC_FATAL(...)        
#endif

#endif


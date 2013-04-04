#include "config.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <hpss_dirent.h>
#include <u_signed64.h>
#include "hpssclapiext.h"
#include <hpss_api.h>
#include <api_internal.h>

/*
 *  Prototypes for static functions.
 */

static int
HPSSFSAL_Common_ReadAttrs(apithrdstate_t * ThreadContext,
                          sec_cred_t * UserCred,
                          ns_ObjHandle_t * ObjHandlePtr,
                          unsigned32 ChaseOptions,
#if HPSS_LEVEL >= 7322
                          unsigned32 OptionFlags,
#endif
                          u_signed64 OffsetIn,
                          unsigned32 BufferSize,
                          unsigned32 GetAttributes,
                          unsigned32 IgnInconstitMd,
                          unsigned32 * End,
                          u_signed64 * OffsetOut, ns_DirEntry_t * DirentPtr);

/*============================================================================
 *
 * Function:    HPSSFSAL_ReadRawAttrsHandle
 *
 * Synopsis:
 *
 * int
 * HPSSFSAL_ReadRawAttrsHandle(
 * ns_ObjHandle_t          *ObjHandle,     ** IN - directory object handle
 * u_signed64              OffsetIn,       ** IN - directory position
 * sec_cred_t              *Ucred,         ** IN - user credentials
 * unsigned32              BufferSize,     ** IN - size of output buffer
 * unsigned32              *End,           ** OUT - hit end of directory
 * u_signed64              *OffsetOut,     ** OUT - resulting directory position
 * ns_DirEntry_t           *DirentPtr)     ** OUT - directory entry information
 *
 * Description:
 *
 *      The 'HPSSFSAL_ReadRawAttrsHandle' function fills in the passed buffer
 *      with directory entries, include file/directory attributes,
 *      beginning at the specified directory position. This routine
 *      will NOT try to chase HPSS junctions encountered.
 *
 * Other Inputs:
 *      None.
 *
 * Outputs:
 *      DirentPtr:      Directory entry.  If null string is returned in
 *                      d_name and/or d_namelen == 0, we hit end of directory.
 *      Return Value:
 *              0               - No error. 'DirentPtr' contains directory
 *                                information.
 *
 * Interfaces:
 *      API_ClientAPIInit
 *      API_ENTER
 *      API_RETURN
 *      HPSSFSAL_Common_ReadAttrs
 *
 * Resources Used:
 *
 * Limitations:
 *
 * Assumptions:
 *
 * Notes:
 *
 *-------------------------------------------------------------------------*/

int HPSSFSAL_ReadRawAttrsHandle(ns_ObjHandle_t * ObjHandle,     /* IN - directory object handle */
                                u_signed64 OffsetIn,    /* IN - directory position */
                                sec_cred_t * Ucred,     /* IN - user credentials */
                                unsigned32 BufferSize,  /* IN - size of output buffer */
                                unsigned32 GetAttributes,       /* IN - get object attributes? */
                                unsigned32 IgnInconstitMd,      /* IN - ignore in case of inconstitent MD */
                                unsigned32 * End,       /* OUT - hit end of directory */
                                u_signed64 * OffsetOut, /* OUT - resulting directory position */
                                ns_DirEntry_t * DirentPtr)      /* OUT - directory entry information */
{
  volatile long error = 0;
  sec_cred_t *ucred_ptr;
  apithrdstate_t *threadcontext;
  static char function_name[] = "HPSSFSAL_ReadRawAttrsHandle";

  API_ENTER(function_name);

  /*
   *  Initialize the thread if not already initialized.
   *  Get a pointer back to the thread specific context.
   */

  error = API_ClientAPIInit(&threadcontext);
  if(error != 0)
    {
      API_RETURN(error);
    }

  /*
   * Make sure that we got a positive buffer size and a
   * valid object handle.
   */

  if(ObjHandle == NULL || BufferSize == 0)
    {
      API_RETURN(-EINVAL);
    }

  /*
   *  Make sure we got a non-NULL dirent pointer.
   */

  if((DirentPtr == NULL) || (End == NULL) || (OffsetOut == NULL))
    {
      API_RETURN(-EFAULT);
    }

  /*
   *  If user credentials were not passed, use the ones in the
   *  current thread context.
   */

  if(Ucred == (sec_cred_t *) NULL)
    ucred_ptr = &threadcontext->UserCred;
  else
    ucred_ptr = Ucred;

  error = HPSSFSAL_Common_ReadAttrs(threadcontext,
                                    ucred_ptr,
                                    ObjHandle,
                                    API_CHASE_NONE,
#if HPSS_LEVEL >= 7322
                                    NS_READDIR_FLAGS_NFS,
#endif
                                    OffsetIn,
                                    BufferSize,
                                    GetAttributes,
                                    IgnInconstitMd, End, OffsetOut, DirentPtr);
  API_RETURN(error);
}

/*============================================================================
 *
 * Function:    HPSSFSAL_Common_ReadAttrs
 *
 * Synopsis:
 *
 * static int
 * HPSSFSAL_Common_ReadAttrs(
 * apithrdstate_t          *ThreadContext, ** IN  - thread context
 * sec_cred_t              *UserCred,      ** IN  - user credentials
 * ns_ObjHandle_t          *ObjHandlePtr,  ** IN  - ID of object
 * unsigned32              ChaseOptions,   ** IN  - chase junctions ?
 * unsigned32              OptionFlags     ** IN - readdir option flags
 * u_signed64              OffsetIn        ** IN  - starting directory offset
 * unsigned32              BufferSize      ** IN  - size of entry buffer
 * unsigned32              GetAttributes,  ** IN  - get object attributes?
 * unsigned32              *End            ** OUT - hit end-of-directory?
 * u_signed64              *OffsetOut      ** OUT - offset of next entry
 * ns_DirEntry_t           *DirentPtr)     ** OUT - returned entry info
 *
 * Description:
 *
 *      The 'HPSSFSAL_Common_ReadAttrs' function performs the common processing
 *      for the calls that return directory entry and entry attribute
 *      information.
 *
 * Other Inputs:
 *      None.
 *
 * Outputs:
 *              < 0  - Error.
 *              >= 0 - Number of entries returned.
 *
 * Interfaces:
 *      API_AddAllRegisterValues
 *      API_core_ReadDir
 *      API_DEBUG_FPRINTF
 *      API_GetUniqueRequestID
 *      API_RemoveRegisterValues
 *      API_TraversePath
 *      cast64m
 *      free
 *      memset
 *      sizeof
 *
 * Resources Used:
 *
 * Limitations:
 *
 * Assumptions:
 *
 * Notes:
 *
 *-------------------------------------------------------------------------*/

static int
HPSSFSAL_Common_ReadAttrs(apithrdstate_t * ThreadContext,
                          sec_cred_t * UserCred,
                          ns_ObjHandle_t * ObjHandle,
                          unsigned32 ChaseOptions,
#if HPSS_LEVEL >= 7322
                          unsigned32 OptionFlags,
#endif
                          u_signed64 OffsetIn,
                          unsigned32 BufferSize,
                          unsigned32 GetAttributes,
                          unsigned32 IgnInconstitMd,
                          unsigned32 * End,
                          u_signed64 * OffsetOut, ns_DirEntry_t * DirentPtr)
{
  int cnt;
  ns_DirEntryConfArray_t direntbuf;
  ns_DirEntry_t *direntptr;
  long error = 0;
  char function_name[] = "HPSSFSAL_Common_ReadAttrs";
  unsigned32 i;
#if HPSS_LEVEL >= 622
  unsigned32 entry_cnt;
#endif
  ns_DirEntry_t *outptr;
  hpss_reqid_t rqstid;
  u_signed64 select_flags;

#if HPSS_LEVEL >= 622
  /* figure out how many entries will fit in the clients buffer */
  entry_cnt = BufferSize / sizeof(ns_DirEntry_t);
#endif

  direntbuf.DirEntry.DirEntry_len = 0;
  direntbuf.DirEntry.DirEntry_val = NULL;

  if(GetAttributes)
    {
      /*
       * Ask for all the attributes that are managed by the
       * Name Service. Start by setting all the bits in
       * the select flags, then clear the ones that are specific
       * to the Bitfile Service.
       */

      select_flags = API_AddAllRegisterValues(MAX_CORE_ATTR_INDEX);
      select_flags = API_RemoveRegisterValues(select_flags,
#if HPSS_MAJOR_VERSION < 7
                                              CORE_ATTR_DM_DATA_STATE_FLAGS,
                                              CORE_ATTR_DONT_PURGE,
#endif
                                              CORE_ATTR_REGISTER_BITMAP,
                                              CORE_ATTR_OPEN_COUNT,
                                              CORE_ATTR_READ_COUNT,
                                              CORE_ATTR_WRITE_COUNT,
                                              CORE_ATTR_TIME_LAST_WRITTEN, -1);
    }
  else
    select_flags = cast64m(0);

  /*
   *  Get a valid request Id and then read the directory entries.
   */

  rqstid = API_GetUniqueRequestID();

  error = API_core_ReadDir(ThreadContext,
                           rqstid,
                           UserCred,
                           ObjHandle,
#if HPSS_LEVEL >= 7322
                           OptionFlags,
#endif
                           OffsetIn,
#if HPSS_LEVEL < 732
                           BufferSize,
#else
                           entry_cnt,
#endif
                           select_flags, End, &direntbuf);

  /* In case of metadata inconsistency, it may return HPSS_ENOENT 
   * when a directory entry has no associated entry in the FS...
   * In this case, we return null object attributes.
   */
  if((error == HPSS_ENOENT) && IgnInconstitMd)
    {
      select_flags = cast64m(0);

      rqstid = API_GetUniqueRequestID();

      error = API_core_ReadDir(ThreadContext,
                               rqstid,
                               UserCred,
                               ObjHandle,
#if HPSS_LEVEL >= 7322
                               OptionFlags,
#endif
                               OffsetIn,
#if HPSS_LEVEL < 732
                               BufferSize,
#else
                               entry_cnt,
#endif
                               select_flags, End, &direntbuf);
    }

  if(error != 0)
    {
      API_DEBUG_FPRINTF(DebugFile, &rqstid,
                        "%s: Could not read directory entries.\n", function_name);
    }

  if(error == 0)
    {

      /*
       *  Now load in the results from the call.
       */

      (void)memset(DirentPtr, 0, BufferSize); /* Todo Why does this crash the daemon ??*/
      cnt = 0;

#if HPSS_LEVEL >= 622
      for(i = 0; i < direntbuf.DirEntry.DirEntry_len && i < entry_cnt; ++i)
#else
      for(i = 0; i < direntbuf.DirEntry.DirEntry_len; ++i)
#endif
        {
          direntptr = &(direntbuf.DirEntry.DirEntry_val[i]);
          outptr = &DirentPtr[cnt++];

          /*
           * If asked to chase junctions and this entry
           * is a junction, return the attributes for
           * the fileset/directory to which the junction
           * points.
           */

          if(((ChaseOptions & API_CHASE_JUNCTION) != 0)
             && direntptr->Attrs.Type == NS_OBJECT_TYPE_JUNCTION)
            {
              hpss_Attrs_t attrs;
              ns_ObjHandle_t obj_handle;
              u_signed64 select_flags;

              memset(&obj_handle, 0, sizeof(obj_handle));
              memset(&attrs, 0, sizeof(attrs));
              select_flags = API_AddAllRegisterValues(MAX_CORE_ATTR_INDEX);

              error = API_TraversePath(ThreadContext,
                                       rqstid,
                                       &ThreadContext->UserCred,
                                       ObjHandle,
                                       (char *)direntptr->Name,
                                       API_NULL_CWD_STACK,
                                       API_CHASE_JUNCTION,
                                       0,
                                       0,
                                       select_flags,
                                       cast64m(0),
                                       API_NULL_CWD_STACK,
                                       &obj_handle, &attrs, NULL, NULL,
#if HPSS_MAJOR_VERSION < 7
                                       NULL,
#endif
                                       NULL, NULL);

              if(error != 0)
                {
                  /*
                   * If we can't find out what the junction points
                   * to, log a message and return the attributes
                   * of the junction itself.
                   */

                  API_DEBUG_FPRINTF(DebugFile, &rqstid,
                                    "HPSSFSAL_Common_ReadAttrs: API_TraversePath"
                                    "failed, error = %d\n", error);
                  error = 0;
                }
              else
                {
                  /*
                   * We got the fileset attributes, copy
                   * them to the entry.
                   */

                  direntptr->ObjHandle = obj_handle;
                  direntptr->Attrs = attrs;
                }
            }

          *outptr = *direntptr;

        }

      if(error == 0)
        {
          if(direntbuf.DirEntry.DirEntry_len > 0)
            *OffsetOut = outptr->ObjOffset;
          else
            *OffsetOut = cast64m(0);

          /*
           *  Return the number of entries returned from the
           *  core server.
           */

          error = cnt;
        }
    }

  if(direntbuf.DirEntry.DirEntry_val != NULL)
    {
      free(direntbuf.DirEntry.DirEntry_val);
    }

  return (error);
}



/*============================================================================
 *
 * Function:    hpss_ReaddirHandle
 *
 * Synopsis:
 *
 * int
 * hpss_ReaddirHandle(
 * ns_ObjHandle_t       *ObjHandle,     ** IN - directory object handle
 * u_signed64           OffsetIn,       ** IN - directory position
 * hsec_UserCred_t      *Ucred,         ** IN - user credentials
 * unsigned32           BufferSize,     ** IN - size of output buffer
 * unsigned32           *End,           ** OUT - hit end of directory
 * u_signed64           *OffsetOut,     ** OUT - resulting directory position
 * hpss_dirent_t        *DirentPtr)     ** OUT - directory entry information
 *
 * Description:
 *
 *      The 'hpss_ReaddirHandle' function fills in the passed buffer with
 *      directory entries beginning at the specified directory position.
 *
 * Other Inputs:
 *      None.
 *
 * Outputs:
 *      DirentPtr:      Directory entry.  If null string is returned in
 *                      d_name and/or d_namelen == 0, we hit end of directory.
 *      Return Value:
 *              0               - No error. 'DirentPtr' contains directory
 *                                information.
 *
 * Interfaces:
 *      API_AddAllRegisterValues
 *      API_ClientAPIInit
 *      API_core_ReadDir
 *      API_DEBUG_FPRINTF
 *      API_ENTER
 *      API_GetUniqueRequestID
 *      API_RETURN
 *      API_TraversePath
 *      cast64
 *      cast64m
 *      free
 *      memset
 *      sizeof
 *      strlen
 *      strncpy
 *
 * Resources Used:
 *
 * Limitations:
 *
 * Assumptions:
 *
 * Notes:
 *
 *-------------------------------------------------------------------------*/

int
HPSSFSAL_ReaddirHandle(
ns_ObjHandle_t  *ObjHandle,     /* IN - directory object handle */
u_signed64      OffsetIn,       /* IN - directory position */
sec_cred_t      *Ucred,         /* IN - user credentials */
unsigned32      BufferSize,     /* IN - size of output buffer */
unsigned32      IgnInconstitMd, /* IN - ignore in case of inconstitent MD */
unsigned32      *End,           /* OUT - hit end of directory */
u_signed64      *OffsetOut,     /* OUT - resulting directory position */
hpss_dirent_t   *DirentPtr)     /* OUT - directory entry information */
{
   int                     cnt;
   volatile long           error = 0;
   hpss_reqid_t            rqstid;
   ns_DirEntry_t           *direntptr;
   ns_DirEntryConfArray_t  direntbuf;
   hpss_dirent_t           *outptr;
   sec_cred_t              *ucred_ptr;
   apithrdstate_t          *threadcontext;
   static char             function_name[] = "hpss_ReaddirHandle";
   unsigned32              i, entry_cnt;



   API_ENTER(function_name);

   /*
    *  Initialize the thread if not already initialized.
    *  Get a pointer back to the thread specific context.
    */

   error = API_ClientAPIInit(&threadcontext);
   if(error != 0)
   {
      API_RETURN(error);
   }


   /*
    * Make sure that we got a positive buffer size and a
    * valid object handle.
    */

   if (ObjHandle == NULL || BufferSize == 0)
   {
      API_RETURN(-EINVAL);
   }


   /*
    *  Make sure we got a non-NULL dirent pointer.
    */

   if ((DirentPtr == NULL) || (End == NULL) || (OffsetOut == NULL))
   {
      API_RETURN(-EFAULT);
   }

   /*
    *  If user credentials were not passed, use the ones in the
    *  current thread context.
    */

   if (Ucred == (sec_cred_t *)NULL)
      ucred_ptr = &threadcontext->UserCred;
   else
      ucred_ptr = Ucred;

   /* figure out how many entries will fit in the clients buffer */
   entry_cnt = BufferSize / sizeof(hpss_dirent_t);

   /*
    *  Get a valid request Id and then read the directory entries.
    */

   rqstid = API_GetUniqueRequestID();

   error = API_core_ReadDir(threadcontext,
                            rqstid,
                            ucred_ptr,
                            ObjHandle,
#if HPSS_LEVEL >= 7322              
                            NS_READDIR_FLAGS_NFS,
#endif
                            OffsetIn,
                            entry_cnt,
                            cast64m(0),
                            End,
                            &direntbuf);

   /* If metadata is wrong, we'll get nothing even if there could be something... Try again */
   if((error == HPSS_ENOENT) && IgnInconstitMd) {
     rqstid = API_GetUniqueRequestID();

     error = API_core_ReadDir(threadcontext,
                              rqstid,
                              ucred_ptr,
                              ObjHandle,
#if HPSS_LEVEL >= 7322              
                              NS_READDIR_FLAGS_NFS,
#endif
                              OffsetIn,
                              entry_cnt,
                              cast64m(0),
                              End,
                              &direntbuf);
   }

   if(error != 0)
   {
      API_DEBUG_FPRINTF(DebugFile, &rqstid,
                        "%s: Could not read directory entries.\n",
                        function_name);
   }

   if(error == 0)
   {

      /*
       *  Now load in the results from the call.
       */

      (void)memset(DirentPtr,0,BufferSize);
      cnt = 0;

      /* march through each dir entry returned, being careful not to
       * go beyond what the client asked for. We do this extra check
       * because the dirent structure could be padded differently
       * depending on the platform, so the server could actually
       * give us more than we can safely stuff in the client's
       * buffer.
       */ 
      for ( i = 0; i < direntbuf.DirEntry.DirEntry_len && i < entry_cnt; ++i )
      {
	 direntptr = &(direntbuf.DirEntry.DirEntry_val[i]);
         outptr = &DirentPtr[cnt++];

         /*
          * If this entry is a junction return
          * the attributes for the fileset/directory
          * to which the junction points.
          */

         if ( direntptr->Attrs.Type == NS_OBJECT_TYPE_JUNCTION )
         {
            hpss_Attrs_t   attrs;
            ns_ObjHandle_t obj_handle;
            u_signed64     select_flags;

            memset(&obj_handle,0,sizeof(obj_handle));
            memset(&attrs,0,sizeof(attrs));
            select_flags = API_AddAllRegisterValues(MAX_CORE_ATTR_INDEX);

            error = API_TraversePath(threadcontext,
                                     rqstid,
				     ucred_ptr,
                                     ObjHandle,
                                     (char *)direntptr->Name,
                                     API_NULL_CWD_STACK,
                                     API_CHASE_JUNCTION,
                                     0,
                                     0,
                                     select_flags,
                                     cast64m(0),
                                     API_NULL_CWD_STACK,
                                     &obj_handle,
                                     &attrs,
				     NULL,
                                     NULL,
                                     NULL,
                                     NULL);

            if (error != 0)
            {
               /*
                * If we can't find out what the junction points
                * to, log a message and return the attributes
                * of the junction itself.
                */

               API_DEBUG_FPRINTF(DebugFile, &rqstid,
                                 "hpss_Readdir: API_TraversePath"
                                 "failed, error = %d\n",error);
               error = 0;
            }
            else
            {
               /*
                * We got the fileset attributes, copy
                * them to the entry.
                */

               direntptr->ObjHandle = obj_handle;
               direntptr->Attrs = attrs;
            }
         }

         outptr->d_offset = direntptr->ObjOffset;
         outptr->d_reclen = 0;
         strncpy((char *)outptr->d_name,
                 (char *)direntptr->Name,
                 HPSS_MAX_FILE_NAME);
         outptr->d_namelen = strlen((char *)outptr->d_name);
         outptr->d_handle = direntptr->ObjHandle;

      }


      if (direntbuf.DirEntry.DirEntry_len > 0)
         *OffsetOut = outptr->d_offset;
      else
         *OffsetOut = cast64(0);

      /*
       * If the result returned includes the last entry, but we
       * will not be returning that entry to the client (because of
       * dirent structure size different between client and server),
       * clear the 'End' flag.
       */

      if(*End && entry_cnt < direntbuf.DirEntry.DirEntry_len) 
         *End = 0;

      free(direntbuf.DirEntry.DirEntry_val);
      direntbuf.DirEntry.DirEntry_val = NULL;


      /*
       *  Return the number of entries returned from the
       *  core server.
       */

      error = cnt;
   }

   API_RETURN(error);
}

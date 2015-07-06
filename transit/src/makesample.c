/****************************** START LICENSE ******************************
Transit, a code to solve for the radiative-transifer equation for
planetary atmospheres.

This project was completed with the support of the NASA Planetary
Atmospheres Program, grant NNX12AI69G, held by Principal Investigator
Joseph Harrington. Principal developers included graduate students
Patricio E. Cubillos and Jasmina Blecic, programmer Madison Stemm, and
undergraduate Andrew S. D. Foster.  The included
'transit' radiative transfer code is based on an earlier program of
the same name written by Patricio Rojo (Univ. de Chile, Santiago) when
he was a graduate student at Cornell University under Joseph
Harrington.

Copyright (C) 2015 University of Central Florida.  All rights reserved.

This is a test version only, and may not be redistributed to any third
party.  Please refer such requests to us.  This program is distributed
in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.

Our intent is to release this software under an open-source,
reproducible-research license, once the code is mature and the first
research paper describing the code has been accepted for publication
in a peer-reviewed journal.  We are committed to development in the
open, and have posted this code on github.com so that others can test
it and give us feedback.  However, until its first publication and
first stable release, we do not permit others to redistribute the code
in either original or modified form, nor to publish work based in
whole or in part on the output of this code.  By downloading, running,
or modifying this code, you agree to these conditions.  We do
encourage sharing any modifications with us and discussing them
openly.

We welcome your feedback, but do not guarantee support.  Please send
feedback or inquiries to:

Joseph Harrington <jh@physics.ucf.edu>
Patricio Cubillos <pcubillos@fulbrightmail.org>
Jasmina Blecic <jasmina@physics.ucf.edu>

or alternatively,

Joseph Harrington, Patricio Cubillos, and Jasmina Blecic
UCF PSB 441
4111 Libra Drive
Orlando, FL 32816-2385
USA

Thank you for using transit!
******************************* END LICENSE ******************************/

#include <transit.h>

/* \fcnfh DEF
   Create a sampling array. Take values from hint or else from a 
   reference sampling.

   Get units factor.  Get inital and final values.
   Check that there is a non-zero interval.
   Check that only the spacing or the number of points have been defined.
   If numpoints was provided, use the given array of values and ommit
   oversampling.  If spacing was provided, calculate numpoints, oversample,
   and fill in values.

   Return: 1 for modified initial value
           2 for modified final   value
           0 if nothing was changed but there is a sampled array
          -1 if hinted initial is bigger than maximum allowed
          -2 if hinted final is smaller than minimum allowed
          -3 if accepted initial value is greater or equal to final one
          -4 if both spacing and number of elements were hinted
          -5 if none or both of spacing or number of elements were in
             the referenced
          -6 Not valid oversampling was given by ref when requested      */
int
makesample1(prop_samp *samp,       /* transit sampling    */
            prop_samp *ref,        /* reference sampling  */
            const long fl){        /* Sampling flag       */

  int res=0;              /* Return output             */
  PREC_RES *v;            /* The sampling values       */
  double osd,             /* Oversampled delta         */
         si;              /* Sample initial value      */
  _Bool dhint=ref->d!=0;  /* True if hint.d is defined */
  /* Acceptable ratio exceeding final value without truncating the last bin: */
  double okfinalexcess = 1e-8;

  /* Get units factor: */
  samp->fct = ref->fct;

  /* Get initial value: */
  si = samp->i = ref->i;

  /* Get final value: */
  samp->f = ref->f;
  /* Check non-zero interval: */
  if (samp->f < samp->i){
    transiterror(TERR_SERIOUS|TERR_ALLOWCONT,
                 "Hinted final value for %s sampling (%g) is smaller than "
                 "hinted initial value %.8g.\n", TRH_NAME(fl),
                 samp->f, samp->i);
    return -3;
  }

  /* Progress report: print flag, spacing, and number of points from hint: */
  transitprint(21, verblevel, "Flags: 0x%lx    hint.d: %g   hint.n: %li\n",
               fl, ref->d, ref->n);

  if (!dhint){
    /* If none of the ref exists then throw error: */
    transiterror(TERR_SERIOUS|TERR_ALLOWCONT,
                 "Spacing (%g) was not hinted in %s sampling.\n",
                 ref->d, TRH_NAME(fl));
    return -5;
  }
  /* If spacing exists then trust it: */
  else if(ref->d!=0){
      samp->d = ref->d;
  }
  /* Else: */
  else{
    transiterror(TERR_SERIOUS, "Invalid spacing (%g) in %s sampling.\n",
                 samp->d, TRH_NAME(fl));
  }

  /* Make sampling based on spacing:       */
  if(samp->d<0) okfinalexcess = -okfinalexcess;
  /* Define number of points:              */
  samp->n = ((1.0+okfinalexcess)*samp->f - samp->i)/samp->d+1;
  if(samp->n<0)
    samp->n = -samp->n;

  /* Check for hinted oversampling:        */
  if(ref->o <= 0){
    transiterror(TERR_SERIOUS|TERR_ALLOWCONT,
                 "Invalid hinted oversampling for %s sampling.\n",
                 TRH_NAME(fl));
    return -6;
  }
  samp->o = ref->o;

  /* Oversampled number of points:      */
  samp->n = (samp->n-1)*samp->o+1;
  /* Oversampled delta:                 */
  osd = samp->d/(double)samp->o;

  /* Allocate and fill sampling values: */
  v = samp->v = (PREC_RES *)calloc(samp->n, sizeof(PREC_RES));
  /* Fill-in values:                    */
  PREC_NREC n = samp->n;
  *v = si;
  v += --n;
  while(n)
    *v-- = si + n--*osd;
  /* FINDME: Why so complicated? This is far easier:
  for (i=0; i<samp->n; i++)
    *v+i = samp->i + i*osd               */

  /* Check the final point: */
  if(si!=0 && samp->v[samp->n-1]!=samp->f && verblevel>2)
    /* FINDME: Consider removig the verblevel condition */
    transiterror(TERR_WARNING,
                 "Final sampled value (%g) of the %li points doesn't coincide "
                 "exactly with required value (%g). %s sampling with "
                 "pre-oversampling spacing of %g.\n", samp->v[samp->n-1],
                 samp->n, samp->f, TRH_NAME(fl), samp->d);

  /* Return the flags of accepted values: */
  return res;
}


/* \fcnfh  DEF
   Create a sampling array. Take values from hint or else from a 
   reference sampling.

   Get units factor.  Get inital and final values.
   Check that there is a non-zero interval.
   Check that only the spacing or the number of points have been defined.
   If numpoints was provided, use the given array of values and ommit
   oversampling.  If spacing was provided, calculate numpoints, oversample,
   and fill in values.

   Return: 1 for modified initial value
           2 for modified final   value
           0 if nothing was changed but there is a sampled array
          -1 if hinted initial is bigger than maximum allowed
          -2 if hinted final is smaller than minimum allowed
          -3 if accepted initial value is greater or equal to final one
          -4 if both spacing and number of elements were hinted
          -5 if none or both of spacing or number of elements were in
             the referenced
          -6 Not valid oversampling was given by ref when requested      */
int
makesample(prop_samp *samp,  /* transit sampling                            */
           prop_samp *hint,  /* hint    sampling                            */
           prop_samp *ref,   /* reference sampling                          */
           const long fl){   /* Sampling flag                               */

  int res=0;              /* Return output                                  */
  PREC_RES *v;            /* The sampling values                            */
  double osd,             /* Oversampled delta                              */
         si;              /* Sample initial value                           */
  _Bool dhint=hint->d!=0; /* True if hint.d is defined                      */
  /* Acceptable ratio exceeding final value without truncating the last bin: */
  double okfinalexcess = 1e-8;

  /* Get units factor:                                                      */
  if(hint->fct<=0)
    samp->fct = ref->fct;
  else
    samp->fct = hint->fct;

  /* Set initial value:                                                     */
  /* If hint unset, use reference:                                          */
  if(hint->i <= 0){
    samp->i = ref->i;
    transitprint(4, verblevel,
                 "Using ref sampling %g [cgs] for initial value of %s.\n",
                 samp->i*samp->fct, TRH_NAME(fl));
    res |= 0x1;  /* Turn on modification flag for return output             */
  }
  else
    samp->i = hint->i;

  /* Set final value (if hint unset, use reference):                        */
  if (hint->f <= 0){
    samp->f = ref->f;
    transitprint(4, verblevel,
                 "Using ref sampling %g [cgs] for final value of %s.\n",
                  samp->f*samp->fct, TRH_NAME(fl));
    res |= 0x2; /* Turn on modification flag for return output              */
  } else
    samp->f = hint->f;

  /* Print flag, spacing, and number of points from hint:                   */
  transitprint(21, verblevel, "Flags: 0x%lx    hint.d: %g   hint.n: %li\n",
                              fl, hint->d, hint->n);

  /* If dhint has not been hinted then use ref's:                           */
  if(!dhint){
    /* If none of the ref exists then throw error:                          */
    if((ref->d==0 && ref->n<=0)){
      transiterror(TERR_SERIOUS|TERR_ALLOWCONT,
                   "Spacing (%g) and number of elements (%i) were either both "
                   "or none in the reference for %s sampling. And yes, none "
                   "were hinted.\n", ref->d, ref->n, TRH_NAME(fl));
      return -5;
    }
    /* If spacing exists then trust it:                                     */
    if(ref->d!=0){
      samp->d = ref->d;
    }
    /* Else, use set array: */
    else{
      /* If initial or final value were modified, then print out a warning: */
      if(res){
        transiterror(TERR_WARNING,
                     "Array of length %i was given as reference for %s "
                     "sampling, but the initial (%g -> %g) or final "
                     "(%g -> %g) values MIGHT have been modified.\n",
                     ref->n, TRH_NAME(fl), ref->i, samp->i, ref->f, samp->f);
      }
      samp->n = ref->n;
      samp->d = 0;
      samp->v = (PREC_RES *)calloc(samp->n, sizeof(PREC_RES));
      memcpy(samp->v, ref->v, samp->n*sizeof(PREC_RES));
      if(ref->o != 0)
        transiterror(TERR_WARNING,
                     "Fixed sampling array of length %i was referenced. " 
                     "But also oversampling was given (%li), ignoring it "
                     "in %s sampling.\n", samp->n, ref->o, TRH_NAME(fl));
      samp->o = 0;
      /* Return any possible modification: */
      return res;
    }
  }
  /* Else if spacing was hinted, then it has to be positive at this point: */
  else if(dhint){
    transitASSERT(hint->d <= 0, "Error: Logic test 1 failed in %s's "
                                "makesample()\n", TRH_NAME(fl));
    samp->d = hint->d;
  }
  else{
    /* n can't be hinted: */
    transiterror(TERR_SERIOUS, "Invalid sampling inputs.\n");
  }

  /* Check non-zero interval: */
  if((samp->f <= samp->i && samp->d > 0) ||
     (samp->f >= samp->i && samp->d < 0)){
    transiterror(TERR_SERIOUS|TERR_ALLOWCONT,
                 "Initial accepted sampling value (%g) is greater or equal "
                 "than final accepted sample value (%g). %s was being "
                 "hinted.\n", samp->i, samp->f, TRH_NAME(fl));
    return -3;
  }

  /* Make sampling based on spacing:       */
  if(samp->d<0) okfinalexcess = -okfinalexcess;
  /* Define number of points:              */
  samp->n = ((1.0+okfinalexcess)*samp->f - samp->i)/samp->d + 1;
  if(samp->n<0)  /* FINDME: Explain how can this happen */
    samp->n = -samp->n;

  /* Check for hinted oversampling:        */
  if(hint->o<=0){
    /* If not, check for ref oversampling: */
    if(ref->o<=0){  /* If not, throw error */
      transiterror(TERR_SERIOUS|TERR_ALLOWCONT,
                   "Not valid oversampling in the reference for "
                   "%s sampling.\n", TRH_NAME(fl));
      return -6;
    }
    samp->o = ref->o;
  }
  else
    samp->o = hint->o;

  /* Oversampled number of points:      */
  samp->n = (samp->n - 1)*samp->o + 1;
  /* Oversampled delta:                 */
  osd = samp->d/(double)samp->o;

  /* Allocate and fill sampling values: */
  si = samp->i;  /* FINDME: delete si and keep using samp->i */
  v = samp->v = (PREC_RES *)calloc(samp->n, sizeof(PREC_RES));
  /* Fill-in values:                    */
  PREC_NREC n = samp->n;
  *v = si;
  v += --n;
  while(n)
    *v-- = si + n--*osd;
  /* FINDME: Why so complicated? This is far easier:
  for (i=0; i<samp->n; i++)
    *v+i = samp->i + i*osd               */

  /* Check the final point: */
  if(si != 0  &&  samp->v[samp->n-1] != samp->f  &&  verblevel > 2)
    /* FINDME: Consider removig the verblevel condition */
    transiterror(TERR_WARNING,
                 "Final sampled value (%g) of the %li points doesn't coincide "
                 "exactly with required value (%g). %s sampling with "
                 "pre-oversampling spacing of %g.\n", samp->v[samp->n-1],
                 samp->n, samp->f, TRH_NAME(fl), samp->d);

  /* Return the flags of accepted values: */
  return res;
}


/* \fcnfh  DEF
   Call makesample with the appropiate parameters and set the flags
   for a wavenumber sampling

   Return: makesample() output                                              */
int
makewnsample(struct transit *tr){
  struct transithint *th = tr->ds.th;
  prop_samp *hsamp  = &th->wns;          /* Hinted wavenumber sampling      */
  prop_samp *wlsamp = &th->wavs;         /* Hinted wavelength sampling      */
  prop_samp rsamp;                       /* Reference wavenumber sampling   */
  memset(&rsamp, 0, sizeof(prop_samp));
  int res;                               /* Result status                   */

  /* Get initial wavenumber value:                                          */
  if (hsamp->i > 0){
    if (hsamp->fct <= 0)
      transiterror(TERR_SERIOUS, "User specified wavenumber factor is "
                                 "negative (%g).\n", hsamp->fct);
    rsamp.i = hsamp->i*hsamp->fct;
    transitprint(1, verblevel, "wave i1: %.3f = %.2f * %.2f\n", rsamp.i, 
                 hsamp->i, hsamp->fct);
  }
  else if (wlsamp->f > 0){
    if (wlsamp->fct <= 0)
      transiterror(TERR_SERIOUS, "User specified wavelength factor is "
                                 "negative (%g).\n", wlsamp->fct);
    rsamp.i = 1.0/(wlsamp->f*wlsamp->fct);
  }
  else
    transiterror(TERR_SERIOUS, "Initial wavenumber (nor final wavelength) "
                               "were correctly provided by the user.\n");

  /* Get final wavenumber value:                                            */
  if (hsamp->f > 0){
    if (hsamp->fct < 0)
      transiterror(TERR_SERIOUS, "User specified wavenumber factor is "
                                 "negative (%g).\n", hsamp->fct);
    rsamp.f = hsamp->f*hsamp->fct;
  }
  else if (wlsamp->i > 0){
    if (wlsamp->fct < 0)
      transiterror(TERR_SERIOUS, "User specified wavelength factor is "
                                 "negative (%g).\n", wlsamp->fct);
    rsamp.f = 1.0/(wlsamp->i*wlsamp->fct);
  }
  else
    transiterror(TERR_SERIOUS, "Final wavenumber (nor initial wavelength) "
                               "were correctly provided by the user.\n");

  /* Set up reference wavenumber sampling:                                  */
  rsamp.o = hsamp->o;

  /* Reference's wavenumber units are cm-1:                                 */
  rsamp.fct = 1;

  /* Don't set the number of elements:                                      */
  rsamp.n = 0;

  /* Set spacing such that the wavenumber grid has the same
     number of points as the wavelength grid:                */
  if (hsamp->d <= 0)
    transiterror(TERR_SERIOUS,
                 "Incorrect wavenumber spacing (%g), it must be positive.\n",
                 hsamp->d);
  rsamp.d = hsamp->d;

  /* Make the oversampled wavenumber sampling:                              */
  res = makesample1(&tr->owns, &rsamp, TRH_WN);
  /* Make the wavenumber sampling:                                          */
  rsamp.o = 1;
  res = makesample1(&tr->wns,  &rsamp, TRH_WN);
  /* Get the exact divisors of the oversampling factor:                     */
  tr->odivs = divisors(tr->owns.o, &tr->ndivs);
  transitprint(20, verblevel, "There are %i divisors of the oversampling "
                              "factor (%i):\n", tr->ndivs, tr->owns.o);
  for (int i=0; i<tr->ndivs; i++)
    transitprint(25, verblevel, "%5i", tr->odivs[i]);
  transitprint(25, verblevel, "\n");

  /* Set progress indicator if sampling was successful and return status:   */
  if(res >= 0)
    tr->pi |= TRPI_MAKEWN;
  return res;
}


/* \fcnfh  DEF
 Calls makesample with the appropiate parameters and set the flags

 @returns makesample() output
          1 Only one point value was requested                              */
int
makeradsample(struct transit *tr){
  int res,    /* Return output                                              */
      i, j,   /* Auxiliary for-loop indices                                 */
      iso1db; /* First isotope's index in a database                        */
  struct lineinfo *li=tr->ds.li;    /* Lineinfo struct                      */
  struct atm_data *atms=tr->ds.at;  /* Atmosphere data struct               */
  struct isotopes  *iso=tr->ds.iso; /* Isotopes struct                      */
  struct molecules *mol=tr->ds.mol; /* Molecules struct                     */

  int nrad,            /* Number of radii to sample                         */
      niso=iso->n_i,   /* Number of isotopes                                */
      ndb =iso->n_db,  /* Number of data bases                              */
      nmol=mol->nmol;  /* Number of molecules                               */

  long flag;                      /* Flag for interpolation type            */
  prop_isov *isovs;               /* lineinfo's isotopes information        */
  prop_samp *rsamp = &atms->rads; /* Atmosphere's radii sampling            */
  prop_samp *rad   = &tr->rads;   /* Output radius sampling                 */
  prop_atm  *atmt  = &tr->atm;    /* Array to store p, t, and mm sampling   */
  prop_mol  *molec = mol->molec;  /* Molecular variable information         */


  /* Check that getatm() and readinfo_tli() have been already called:       */
  transitcheckcalled(tr->pi, "makeradsample", 2, "getatm",       TRPI_GETATM,
                                                 "readinfo_tli", TRPI_READINFO);
  /* Exception for re-runs:                                                 */
  if (tr->pi & TRPI_MAKERAD){
    /* Free memory before re-allocating them:                               */
    free_atm(atmt);
    for (i=0; i<nmol; i++)
      free_mol(mol->molec+i);
    for (i=0; i<niso; i++)
      free_isov(iso->isov+i);
    freemem_samp(rad);
    tr->pi &= ~(TRPI_MAKERAD);
  }
  /* Check that variables are not NULL:                                     */
  transitASSERT(atms->rads.n<1 || !ndb || !niso || !nmol,
                "makeradsample():: called but essential variables are "
                "missing (%d, %d, %d, %d).\n", atms->rads.n, ndb, niso, nmol);

  /* Set interpolation function flag:                                       */
  flag = tr->interpflag;
  transitprint(30, verblevel, "transit interpolation flag: %li.\n", flag);

  /* We need to set-up limit so that the hinted values are compatible
     with the atmosphere                                                    */
 
  /* If there is only one atmospheric point, don't do makesample:           */
  if(rsamp->n==1){
    rad->n    = 1;
    rad->i    = rsamp->i;
    rad->f    = rsamp->f;
    rad->fct  = rsamp->fct;
    rad->d    = 0;
    rad->v    = (PREC_RES *)calloc(1, sizeof(PREC_RES));
    rad->v[0] = rsamp->v[0]; 
    res       = 0;   /* makesample()-like output                            */
    /* FINDME: warn that hinted values are going to be useless              */
  }
  /* Keep atmospheric-file sampling:                                        */
  else if (tr->ds.th->rads.d == -1){
    rad->n    = rsamp->n;
    rad->i    = rsamp->i;
    rad->f    = rsamp->f;
    rad->fct  = rsamp->fct;
    rad->d    = 0;
    rad->v    = (PREC_RES *)calloc(rad->n, sizeof(PREC_RES));
    for (i=0; i < rad->n; i++)
      rad->v[i] = rsamp->v[i];
    res       = 0;
  }
  /* Resample to equidistant radius array:                                  */
  else{
    res = makesample(rad, &tr->ds.th->rads, rsamp, TRH_RAD);
  }
  nrad = rad->n;

  /* Allocate arrays that will receive the interpolated data:               */
  for(i=0; i<nmol; i++){
    molec[i].d = (PREC_ATM *)calloc(nrad, sizeof(PREC_ATM));
    molec[i].q = (PREC_ATM *)calloc(nrad, sizeof(PREC_ATM));
    molec[i].n = nrad;
  }
  for (i=0; i<niso; i++){
    iso->isov[i].z = (PREC_ZREC *)calloc(nrad, sizeof(PREC_ZREC));
    iso->isov[i].n = nrad;
  }

  atmt->tfct = atms->atm.tfct;
  atmt->pfct = atms->atm.pfct;
  atmt->t  = (PREC_ATM *)calloc(nrad, sizeof(PREC_ATM));
  atmt->p  = (PREC_ATM *)calloc(nrad, sizeof(PREC_ATM));
  atmt->mm = (double   *)calloc(nrad, sizeof(double));

  /* Interpolate the atm. temperature, pressure, and mean molecular mass:   */
  atmt->t  = splinterp(rsamp->n, rsamp->v, atms->atm.t, nrad, rad->v, atmt->t);
  atmt->p  = splinterp(rsamp->n, rsamp->v, atms->atm.p, nrad, rad->v, atmt->p);
  atmt->mm = splinterp(rsamp->n, rsamp->v, atms->mm,    nrad, rad->v, atmt->mm);

  /* Temperature boundary check:                                            */
  for (i=0; i<nrad; i++){
    if (atmt->t[i] < li->tmin)
      transiterror(TERR_SERIOUS, "The layer %d in the atmospheric model has "
                   "a lower temperature (%.1f K) than the lowest allowed "
                   "TLI temperature (%.1f K).\n", i, atmt->t[i], li->tmin);
    if (atmt->t[i] > li->tmax)
      transiterror(TERR_SERIOUS, "The layer %d in the atmospheric model has "
                   "a higher temperature (%.1f K) than the highest allowed "
                   "TLI temperature (%.1f K).\n", i, atmt->t[i], li->tmax);
  }

  /* Interpolate molecular density and abundance:                           */
  for(i=0; i<nmol; i++){
    molec[i].d = splinterp(rsamp->n, rsamp->v, atms->molec[i].d,
                           nrad,     rad->v,   molec[i].d);
    molec[i].q = splinterp(rsamp->n, rsamp->v, atms->molec[i].q,
                           nrad,     rad->v,   molec[i].q);
  }

  /* Interpolate isotopic partition function and cross section:             */
  for(i=0; i<ndb; i++){       /* For each database separately:              */
    iso1db = iso->db[i].s;    /* Index of first isotope in current DB       */
    isovs  = li->isov + iso1db;
    for(j=0; j < iso->db[i].i; j++){
      transitASSERT(iso1db + j > niso-1,
                    "Trying to reference an isotope (%i) outside the extended "
                    "limit (%i).\n", iso1db+j, niso-1);
      iso->isov[iso1db+j].z = splinterp(li->db[i].t, li->db[i].T, isovs[j].z,
                                        nrad, atmt->t, iso->isov[iso1db+j].z);
    }
  }
  /* Set progress indicator and return:                                     */
  if(res>=0)
    tr->pi |= TRPI_MAKERAD;
  return res;
}


/* \fcnfh  DEF
   Calls makesample with the appropiate parameters and set the flags for
   an impact parameter sampling

   @returns makesample() output                                             */
int
makeipsample(struct transit *tr){
  struct transithint *th = tr->ds.th;
  int res=0; /* Return output */
  int i;

  /* Case when I used the atmospheric radius sampling:                      */
  if (th->rads.d == -1){
    tr->ips.n = tr->rads.n;
    tr->ips.d = 0;
    tr->ips.i = tr->rads.f;
    tr->ips.f = tr->rads.i;
    tr->ips.v = (PREC_RES *)calloc(tr->ips.n, sizeof(PREC_RES));
    for(i=0; i < tr->ips.n; i++)
      tr->ips.v[i] = tr->rads.v[tr->ips.n-i-1];
    tr->ips.o = 0;
    tr->ips.fct = tr->rads.fct;
  }
  /* FINDME: This is not even an option                                     */
  else{
    /* Hinted ip sample:                                                    */
    prop_samp usamp = {0, -th->ips.d,    th->ips.f,              th->ips.i,
                       th->ips.o,        NULL,                   th->ips.fct};
    /* Reference ip sample (inverse radius sample):                         */
    prop_samp rsamp = {0, -tr->rads.d,  tr->rads.v[tr->rads.n-1],
                       tr->rads.v[0],   tr->rads.o,   NULL,     tr->rads.fct};

    if(usamp.f < usamp.i)
      transiterror(TERR_SERIOUS,
                 "Wrong specification of impact parameter, final value (%g) "
                 "has to be bigger than initial (%g).\n", usamp.f, usamp.i);

    transitcheckcalled(tr->pi, "makeipsample", 1, "makeradsample",TRPI_MAKERAD);

    /* Make the sampling taking as reference the radius sampling:           */
    res = makesample(&tr->ips, &usamp, &rsamp, TRH_IPRM);
  }

  /* Print sample information to file                                       */
  if (th->savefiles)
    outsample(tr);

  /* Set progress indicator:                                                */
  if(res >= 0)
    tr->pi |= TRPI_MAKEIP;
  return res;
}


/* \fcnfh  DEF
   Calls makesample with the appropiate parameters and set the flags for
   a temperature sampling

   Return: makesample() output                                              */
int
maketempsample(struct transit *tr){
  int res; /* Return output                                                 */
  struct transithint *th = tr->ds.th; /* transithint                        */

  /* Hinted temperature sample:                                             */
  prop_samp usamp = {0, th->temp.d, th->temp.i, th->temp.f, 1, NULL, 1};
  /* Reference temperature sample:                                          */
  prop_samp rsamp = {0, 0,          0,          0,          1, NULL, 1};

  if (usamp.f < usamp.i)
    transiterror(TERR_SERIOUS, "Wrong specification of temperature, final "
        "value (%g) has to be bigger than initial (%g).\n", usamp.f, usamp.i);

  /* Make the sampling:                                                     */
  res = makesample(&tr->temp, &usamp, &rsamp, TRH_TEMP);

  /* Set progress indicator:                                                */
  if (res >= 0)
    tr->pi |= TRPI_MAKETEMP;
  return res;
}


inline double * tri(double *a,
	     double *d,
	     double *c,
             double *b,
	     double *e,
	     long n){
	int i;
	int j;
	double xmult;

	for (i=1; i<=n; i++){
		xmult = a[i-1]/d[i-1];
		d[i] = d[i] - xmult*c[i-1];
		b[i] = b[i] - xmult*b[i-1];
	}

	e[n-1] = b[n-1]/d[n-1];

	for (j = n; j >= 1; j--){
		e[j] = (b[j-1]-c[j-1]*e[j+1])/d[j-1];
	}
	
	e[0] = 0;
	e[n+1] = 0;
	return e;
}

inline double * spline3(double *xi,
		 double *yi,
		 double *x,
		 double *z,
		 double *h,
		 double *y,
		 long nx,
		 long N){

	int i, j, n;
	double dx;
	double amult, bmult, cmult;
	
	y[0] = yi[0];
	for (n=0; n<=nx; n++){
		for (i=0; i<N-1; i++){
			if (xi[i] <= x[n]){
				j = i;
			}
		}
		amult = (z[j+1] - z[j])/(6*h[j]);
		bmult = z[j]/2;
		cmult = (yi[j+1] - yi[j])/h[j] -h[j]/6 * (z[j+1] + 2 * z[j]);
		dx = x[n] - xi[j];
		y[n] = yi[j] + dx*cmult + dx*dx*bmult + dx*dx*dx*amult;
	}
        
	return y;
}

inline double * splinterp(long N,
                   double *xi,
		   double *yi,
		   long nx,
                   double *xout,
		   double *yout){

	double *b;
	double *h;
	double *k;
	double *a;
	double *d;
	double *z;
	double *c;
	double *y;
	int n;

	int i;

	
	/* Account for the endpoints. The code is written to calculate nx 
           points not including the final endpoint. This line makes it so
           the return has the desired number of values                    */
	nx -= 1;

	b  = calloc(N,    sizeof(double));
	h  = calloc(N,    sizeof(double));
	k  = calloc(N,    sizeof(double));
	a  = calloc(N,    sizeof(double));
	d  = calloc(N,    sizeof(double));
	z  = calloc(N+1,  sizeof(double));
	c  = calloc(N,    sizeof(double));
	y  = calloc(nx+1, sizeof(double));

	b[0] = (yi[1] - yi[0]) / (xi[1] - xi[0]);
 
	for (i=0; i<=N-2; i++){
		b[i+1] = -b[i] + 2*(yi[i+1] - yi[i]) / (xi[i+1] - xi[i]);
	}

	N -= 1;

	for (i=0; i<=N-1; i++){
		h[i] = xi[i+1] - xi[i];
	}

	for (i=0; i<=N-2; i++){
	  k[i] = 6*( (yi[i+2] - yi[i+1])/h[i+1] - (yi[i+1] - yi[i])/h[i] );
	}

	for (i=0; i<=N-2; i++){
		a[i] = h[i+1];
	}

	for (i=0; i<=N-2; i++){
		d[i] = 2*(h[i] + h[i+1]);
	}

	N += 1;

	c = a;

	double *e;
	e = calloc(N, sizeof(double));

	z = tri(a, d, c, k, e, N-2);

	yout = spline3(xi, yi, xout, z, h, y, nx, N);

	return yout;
}

double splinterp_pt(double *z, long N, double *x, double *y, double xout, double yout){
  int midindex;
  
  int index;

  int first;
  int last;

  first = 0;
  last  = N;

  midindex = (first + last)/2;

  while(first<=last){
    if(x[midindex] < xout && x[midindex + 1] > xout){
      index = midindex;
      break;
    }
    else if(x[midindex] < xout){
      first = midindex+1;
      midindex = (first + last)/2;
    }
    else if(x[midindex] > xout){
      last  = midindex-1;
      midindex = (first + last)/2;
    }
    else{
      index = midindex;
      break;
    }   
  }

  double x_hi, x_lo;
  double y_hi, y_lo;

  x_lo = x[index];
  x_hi = x[index+1];
  y_lo = y[index];
  y_hi = y[index+1];

  double h;
  double dy;

  h = x_hi - x_lo;
  dy = y_hi - y_lo;

  if(x[index] == xout)
    yout = y[index];
  else if(h > 0){
    double dx;
    double a,b,c;
    dx = xout - x_lo;
    a = (z[index+1] - z[index])/(6*h);
    b = z[index]/2;
    c = dy/h - h/6 * (z[index+1] + 2*z[index]);
    yout = y_lo + dx*(c + dx*(b + dx*a));
  }
  else
    yout = 0;


  return yout;
}

double * spline_init(double *z, double *x, double *y, long N){
  double *a;
  double *d;
  double *b;
  double *h;
  double *k;
  int i;

  a = calloc(N, sizeof(double));
  d = calloc(N, sizeof(double));
  b = calloc(N, sizeof(double));
  h = calloc(N-1, sizeof(double));
  k = calloc(N-1, sizeof(double));

  for(i=0;i<N-1;i++)
    h[i] = x[i+1]-x[i];

  b[0] = (y[1] - y[0]) / (x[1] - x[0]);

  for(i=0;i<N-1;i++)
    b[i+1] = -b[i] + 2*(y[i+1] - y[i]) / (x[i+1] - x[i]);

  for(i=0;i<N-2;i++)
    d[i] = 2*(h[i] + h[i+1]);

  for(i=0;i<N-2;i++)
    a[i] = h[i+1];

  for(i=0;i<N-1;i++)
    k[i] = 6*((y[i+2] - y[i+1])/h[i+1] - (y[i+1] - y[i])/h[i]);

  z = tri(a, d, a, k, z, N-2);

  return z;
}
    

/* Access to i-th value of array a:                                         */
/* FINDME: put this elswhere                                                */

#define INDd(a,i) *((double *)(a + i))

/*
"Calculate the differentials for a Simpson-rule integration.
                                                            
Parameters:                                                 
-----------                                                 
h: 1D double ndarray                                        
   Intervals between the X-axis samples.                    
                                                            
Returns:                                                    
--------                                                    
hsum: 1D double ndarray                                     
   Sums of interval pairs.                                  
hratio: 1D double ndarray                                   
   Ratio of consecutive intervals.                          
hfactor: 1D double ndarray                                  
   Factor interval.                                         
                                                            
Notes:                                                      
------                                                      
- If there are even samples, skip the first interval.       
- hsum    = [h0+h1, h2+h3, h4+h5, ...]                      
- hratio  = [h1/h0, h3/h2, h5/h4, ...]                      
- hfactor = [hsum0*hsum0/h0*h1, hsum1*hsum1/h2*h3, ...]     
" */

inline void 
*geth(double *h, double *hsum, double *hratio, double *hfactor, int n){
  int size;            /* Size of output array                              */
  int i, j, even=0;    /* Auxilliary for-loop indices                       */

  /* Empty array case:                                                      */
  if (n==0){
    hsum    = 0;
    hratio  = 0;
    hfactor = 0;
    return;
  }
    
  /* Check for even number of samples (odd number of intervals):            */
  even = n%2;
  /* Calculate size of h arrays                                             */ 
  size = n/2;

  if(even)
    even = 0;
  else 
    even = 1;

  /* Calculate hsum, hratio, hfactor                                        */
  for (i=0; i<size; i++){
    j = 2*i + even;
    INDd(hsum,   i) = INDd(h,(j  )) + INDd(h,(j+1));
    INDd(hratio, i) = INDd(h,(j+1)) / INDd(h,(j  ));
    INDd(hfactor,i) = INDd(hsum,i)*INDd(hsum,i) / (INDd(h,(j))*INDd(h,(j+1)));
  }
}


/*
"Wrapper for Simpson-rule integration.
                                                              
Parameters:                                                   
-----------                                                   
y: 1D double ndarray                                          
   Function to integrate.                                     
h: 1D double ndarray                                          
   Intervals between function evaluations.                    
hsum: 1D double ndarray                                       
   Sums of interval pairs.                                    
hratio: 1D double ndarray                                     
   Ratio of consecutive intervals.                            
hfactor: 1D double ndarray                                    
   Factor interval.                                           
                                                              
Returns:                                                      
--------                                                      
integ: Float                                                  
   Integral of y over intervals h using the Simpson rule.     
                                                              
Notes:                                                        
------                                                        
- If there are even samples, use a trapezoidal integration for
  the first interval.                                         
- See geth for formula for hsum, hratio, and hfactor");
*/

inline double 
simps(double *y, double *h, double *hsum, double *hratio, double *hfactor, int n){
  int even;
  double integ=0;
  even = n%2 == 0;

  /* Simple case, nothing to integrate:                                     */ 
  if (n == 1)
    return 0.0;
  /* Simple case, do a trapezoidal integration:                             */
  if (n == 2)
    return INDd(h,0) * (INDd(y,0) + INDd(y,1))/2;

  /* Do Simpson integration (skip first if even):                           */
  integ = simpson(y, hsum, hratio, hfactor, n);

  /* Add trapezoidal rule for first interval if n is even:                  */
  if (even){
    integ += INDd(h,0) * (INDd(y,0) + INDd(y,1))/2;
  }

  return integ;
}

/* \fcnfh DEF
   Make a spacing array for integration                                     */
inline void
makeh(double *x, double *h, int n){
  int i;

  /* Calculate spacing between each point                                   */
  for(i=0;i<n-1;i++){
    INDd(h,i) = INDd(x, i+1) - INDd(x, i);
  }
}

/* \fcnfh DEF
   Perform Simpson integration calculation                                  */
inline double
simpson(double *y,         /* Values of function to integrate        */
        double *hsum,      /* See simpson.c's geth function          */
        double *hratio,    /* See simpson.c's geth function          */
        double *hfactor,   /* See simpson.c's geth function          */
        int n){            /* Number of elements in y                */

  /* Do the final sum for the Simpson integration algorithm.  Based on 
     the Python implementation:
     github.com/scipy/scipy/blob/v0.15.1/scipy/integrate/quadrature.py      */

  int i=0,           /* for-loop index                                      */
      j;             /* Array index for each interval                       */
  double res = 0.0;  /* The results                                         */

  /* Add contribution from each interval:                                   */
  for (i=0; i < (n-1)/2; i++){
    /* Skip first value of y if there's an even number of samples:          */
    j = 2*i + (n%2==0);
    res += (INDd(y, (j  )) * (2.0 - INDd(hratio, i))     +
            INDd(y, (j+1)) * INDd(hfactor, i)            +
            INDd(y, (j+2)) * (2.0 - 1.0/INDd(hratio, i)) ) * INDd(hsum,i);
  }

  return res/6.0;
}

/* \fcnfh  DEF
   Print sample info for a structure                                        */
static void
printsample(FILE *out,        /* File pointer to write out                  */
            prop_samp *samp,  /* Sample strucuture to print                 */
            char *desc,       /* Description                                */
            long fl){         /* Various flag                               */
  int i;

  /* Print header:                                                          */
  fprintf(out, "############################\n"
               "   %-12s Sampling\n"
               "----------------------------\n", desc);

  /* Print units, sample limits, and spacing:                               */
  fprintf(out, "Factor to cgs units: %g\n",            samp->fct);
  fprintf(out, "Initial value: %g\nFinal value: %g\n", samp->i, samp->f);
  fprintf(out, "Spacing: %g\n",                        samp->d);

  /* Print oversampling if necessary:                                       */
  if(!(fl&TRF_NOOVERSAMP))
    fprintf(out, "Oversample: %i\n", samp->o);

  /* Print sample size:                                                     */
  fprintf(out, "Number of elements: %li\n", samp->n);

  /* Print sample array:                                                    */
  if(!(fl&TRF_NOVALUE)){
    fprintf(out, "Values: ");
    for(i=0; i<samp->n; i++)
      fprintf(out, " %12.8g", samp->v[i]);
    fprintf(out, "\n");
  }
}


/* \fcnfh  DEF
   Saves in binary the sample structure               */
void
savesample(FILE *out,        /* File pointer to write */
           prop_samp *samp){ /* Sampling strucuture   */
  fwrite(samp, sizeof(prop_samp), 1, out);
  savesample_arr(out, samp);
}


/* \fcnfh  DEF
   Saves in binary the sample structure's arrays */
void
savesample_arr(FILE *out,        /* File pointer to write */
               prop_samp *samp){ /* Sampling strucuture   */
  if(samp->n>0)
    fwrite(samp->v, sizeof(PREC_RES), samp->n, out);
}


/* \fcnfh  DEF
   Restore a binary sample structure

   Return: 0 on success, else
          -1 if not all the expected information is read
          -2 if info read is wrong
          -3 if cannot allocate memory
           1 if information read was suspicious      */
int
restsample(FILE *in,         /* File pointer to read */
           prop_samp *samp){ /* Sampling strucuture  */
  if(fread(samp, sizeof(prop_samp), 1, in) != 1)
    return -1;
  return restsample_arr(in, samp);
}


/* \fcnfh  DEF
   Restore a binary sample structure

   Return: 0 on success, else
          -1 if not all the expected information is read
          -2 if info read is wrong
          -3 if cannot allocate memory
           1 if information read was suspicious             */
int
restsample_arr(FILE *in,         /* File pointer to read */
               prop_samp *samp){ /* Sampling strucuture  */
  if(samp->n<0)
    return -2;
  if(samp->n>1000000)
    return 1;
  if((samp->v = (PREC_RES *)calloc(samp->n, sizeof(PREC_RES)))==NULL)
    return -3;
  if(samp->n==0)
    return 0;
  if(fread(samp->v, sizeof(PREC_RES), samp->n, in) != samp->n)
    return -1;

  return 0;
}


/* \fcnfh  DEF
   Print the sample data to file.

   Return: 0 on success, else
           1 if cannot write to file  */
int
outsample(struct transit *tr){
  FILE *out = stdout;
  char *filename = tr->f_outsample; /* Sample filename */

  /* Check output filename exists: */
  if(!filename)
    return 0;

  /* If default f_outsample, print to screen: */
  if(strcmp(filename, "-") != 0)
    if((out=fopen(filename, "w"))==NULL){
      transiterror(TERR_WARNING, "Cannot open file '%s' for writing sampling "
                                 "data.\n", filename);
      return 1;
    }

  transitprint(1, verblevel, "Printing sampling information in '%s'.\n\n",
               filename);

  /* Print each sample: */
  printsample(out, &tr->wns,  "Wavenumber",       TRF_NOVALUE);
  printsample(out, &tr->wavs, "Wavelength",       TRF_NOVALUE);
  printsample(out, &tr->rads, "Radius",           TRF_NOOVERSAMP);
  printsample(out, &tr->ips,  "Impact parameter", 0);

  fclose(out);

  return 0;
}


/* \fcnfh  DEF
 Frees the sampling structure */
void
freemem_samp(prop_samp *samp){
  if(samp->n)
    free(samp->v);
}


#ifdef DBGSAMPLE
int
main(int argc, char *argv[]){

  prop_samp lim, hint={0,0,0,0,0}, res;
  float mar=0;
  int i;

  if(argc<5){
    fprintf(stderr, "Syntax:\n"
            "    dbgsample <ini> <fin> <delt> <oversampling> [<margin>]\n");
    exit(0);
  }

  lim.i = atof(argv[1]);
  lim.f = atof(argv[2]);
  lim.d = atof(argv[3]);
  lim.o = atof(argv[4]);
  lim.n = 0;
  if(argc==6)
    mar = atof(argv[5]);

  i = makesample(&res, &hint, &lim, 0, 0, mar);

  fprintf(stderr, "Makesample returned %i\n"
          "Initial %g, final %g, delta %g, oversamp %i, number %li, "
          "margin %g\n", i, res.i, res.f, res.d, res.o, res.n, mar);

  if(i<0)
    exit(1);

  for(i=0; i<res.n; i++)
    fprintf(stderr," rad(%i): %g\n", i, res.v[i]);
}

#endif /* debug */

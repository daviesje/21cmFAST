"""
A module providing Core Modules for cosmoHammer. This is the basis of the plugin system for py21cmmc.
"""
import warnings
import py21cmmc as p21


class CoreBase:
    def __init__(self, store=None):
        self.store = store or {}

    def prepare_storage(self, ctx, storage):
        "Add variables to special dict which cosmoHammer will automatically store with the chain."
        for name, storage_function in self.store.items():
            try:
                storage[name] = storage_function(ctx)
            except Exception:
                print("Exception while trying to evaluate storage function %s"%name)
                raise

    @property
    def default_ctx(self):
        try:
            return self.LikelihoodComputationChain.core_context()
        except AttributeError:
            raise AttributeError("default_ctx is not available unless the likelihood is embedded in a LikelihoodComputationChain")


class CoreCoevalModule(CoreBase):
    """
    A Core Module which evaluates coeval cubes at given redshift.

    On each iteration, this module will add to the context:

    1. ``init``: an :class:`~py21cmmc._21cmfast.wrapper.InitialConditions` instance
    2. ``perturb``: a :class:`~py21cmmc._21cmfast.wrapper.PerturbedField` instance
    3. ``xHI``: an :class:`~py21cmmc._21cmfast.wrapper.IonizedBox` instance
    4. ``brightness_temp``: a :class:`~py21cmmc._21cmfast.wrapper.BrightnessTemp` instance
    """
    def __init__(self, redshift,
                 user_params=None, flag_options=None, astro_params=None,
                 cosmo_params=None, regenerate=True, do_spin_temp=False, z_step_factor=1.02,
                 z_heat_max=None, change_seed_every_iter=False, ctx_variables=["brightness_temp", "xHI"],
                 **io_options):
        """
        Initialize the class.

        .. note:: None of the parameters provided here affect the *MCMC* as such; they merely provide a background
                  model on which the MCMC will be performed. Thus for example, passing `HII_EFF_FACTOR=30` in
                  `astro_params` here will be over-written per-iteration if `HII_EFF_FACTOR` is also passed as a
                  `parameter` to an MCMC routine using this Core Module.

        Parameters
        ----------
        redshift : float or array_like
             The redshift(s) at which to evaluate the coeval cubes.
        user_params : dict or :class:`~py21cmmc._21cmfast.wrapper.UserParams`
            Parameters affecting the overall dimensions of the cubes (see :class:`~py21cmmc._21cmfast.wrapper.UserParams`
            for details).
        flag_options : dict or :class:`~py21cmmc._21cmfast.wrapper.FlagOptions`
            Options affecting choices for how the reionization is calculated (see
            :class:`~py21cmmc._21cmfast.wrapper.FlagOptions` for details).
        astro_params : dict or :class:`~py21cmmc._21cmfast.wrapper.AstroParams`
            Astrophysical parameters of reionization (see :class:`~py21cmmc._21cmfast.wrapper.AstroParams` for details).
        cosmo_params : dict or :class:`~py21cmmc._21cmfast.wrapper.CosmoParams`
            Cosmological parameters of the simulations (see :class:`~py21cmmc._21cmfast.wrapper.CosmoParams` for
            details).
        regenerate : bool, optional
            Whether to force regeneration of simulations, even if matching cached data is found.
        do_spin_temp: bool, optional
            Whether to use spin temperature in the calculation, or assume the saturated limit.
        z_step_factor: float, optional
            How large the logarithmic steps between redshift are (if required).
        z_heat_max: float, optional
            Controls the global `Z_HEAT_MAX` parameter, which specifies the maximum redshift up to which heating sources
            are required to specify the ionization field. Beyond this, the ionization field is specified directly from
            the perturbed density field.
        ctx_variables : list of str, optional
            A list of strings, any number of the following: "brightness_temp", "init", "perturb", "xHI". These each
            correspond to an OutputStruct which will be stored in the context on every iteration. Omitting as many as
            possible is useful in that it reduces the memory that needs to be transmitted to each process. Furthermore,
            in-built pickling has a restriction that arrays cannot be larger than 4GiB, which can be easily over-run
            when passing the hires array in the "init" structure.

        Other Parameters
        ----------------
        store :  dict, optional
            The (derived) quantities/blobs to store in the MCMC chain, default empty. See Notes below for details.
        cache_dir : str, optional
            The directory in which to search for the boxes and write them. By default, this is the directory given by
            ``boxdir`` in the configuration file, ``~/.21CMMC/config.yml``. Note that for *reading* data, while the
            specified `direc` is searched first, the default directory will *also* be searched if no appropriate data is
            found in `direc`.
        cache_init : bool, optional
            Whether to cache init and perturb data sets, if cosmology is static. This is done before the parameter
            retention step of an MCMC; i.e. before deciding whether to retain the current set of parameters given
            the previous set, which can be useful in diagnosis. Default True.
        cache_ionize : bool, optional
            Whether to cache ionization data sets (done before parameter retention step). Default False.


        Notes
        -----
        The ``store`` keyword is a dictionary, where each key specifies the name of the resulting data entry in the
        samples object, and the value is a callable which receives the ``context``, and returns a value from it.

        This means that the context can be inspected and arbitrarily summarised before storage. In particular, this
        allows for taking slices of arrays and saving them. One thing to note is that the context is dictionary-like,
        but is not a dictionary. The elements of the context are only available by using the ``get`` method, rather than
        directly subscripting the object like a normal dictionary.

        .. note:: only scalars and arrays are supported for storage in the chain itself.
        """
        super().__init__(io_options.get("store", None))

        self.redshift = redshift
        if not hasattr(self.redshift, "__len__"):
            self.redshift = [self.redshift]

        self.user_params = p21.UserParams(user_params)
        self.flag_options = p21.FlagOptions(flag_options)
        self.astro_params = p21.AstroParams(astro_params)
        self.cosmo_params = p21.CosmoParams(cosmo_params)
        self.change_seed_every_iter = change_seed_every_iter
        self.regenerate = regenerate
        self.ctx_variables = ctx_variables

        self.z_step_factor = z_step_factor
        self.z_heat_max = z_heat_max
        self.do_spin_temp = do_spin_temp

        self.io = dict(
            store={},            # (derived) quantities to store in the MCMC chain.
            cache_dir=None,      # where full data sets will be written/read from.
            cache_init=True,     # whether to cache init and perturb data sets (done before parameter retention step).
            cache_ionize=False,  # whether to cache ionization data sets (done before parameter retention step)
        )

        self.io.update(io_options)

        self.initial_conditions = None
        self.perturb_field = None
        # self._modifying_cosmo = False

    def setup(self):
        """
        Perform setup of the core.

        Notes
        -----
        This method is called automatically by its parent :class:`~LikelihoodComputationChain`, and should not be
        invoked directly.
        """
        self.parameter_names = getattr(self.LikelihoodComputationChain.params, "keys", [])

        # If the chain has different parameter truths, we want to use those for our defaults.
        self._update_params(self.LikelihoodComputationChain.createChainContext({}).getParams())

        if self.z_heat_max is not None:
            p21.global_params.Z_HEAT_MAX = self.z_heat_max

        # Here we initialize the init and perturb boxes.
        # If modifying cosmo, we don't want to do this, because we'll create them
        # on the fly on every iteration.
        if not any([p in self.cosmo_params.self.keys() for p in self.parameter_names]) and not self.change_seed_every_iter:
            print("Initializing init and perturb boxes for the entire chain...", end='', flush=True)
            self.initial_conditions = p21.initial_conditions(
                user_params=self.user_params,
                cosmo_params=self.cosmo_params,
                write=self.io['cache_init'],
                direc=self.io['cache_dir'],
                regenerate=self.regenerate,
            )

            self.perturb_field = []
            for z in self.redshift:
                self.perturb_field += [p21.perturb_field(
                    redshift=z,
                    init_boxes=self.initial_conditions,
                    write=self.io['cache_init'],
                    direc=self.io['cache_dir'],
                    regenerate=self.regenerate,
                )]
            print(" done.")

            # Update the cosmo params to the fully realized ones
            self.cosmo_params.update(RANDOM_SEED=self.initial_conditions.cosmo_params.RANDOM_SEED)

    def __call__(self, ctx):
        # Update parameters
        self._update_params(ctx.getParams())

        # Call C-code
        init, perturb, xHI, brightness_temp = self.run(self.astro_params, self.cosmo_params)

        for key in self.ctx_variables:
            try:
                ctx.add(key, locals()[key])
            except KeyError:
                raise KeyError("ctx_variables must be drawn from the list ['init', 'perturb', 'xHI', 'brightness_temp']")

    def _update_params(self, params):
        """
        Update all the parameter structures which get passed to the driver, for this iteration.

        Parameters
        ----------
        params : Parameter object from cosmoHammer

        """
        # Note that RANDOM_SEED is never updated. It should only change when we are modifying cosmo.
        self.astro_params.update(**{k: getattr(params, k) for k,v in params.items() if k in self.astro_params.defining_dict})
        self.cosmo_params.update(
            **{k: getattr(params, k) for k,v in params.items() if k in self.cosmo_params.defining_dict})

        # We need to reset the seed to None on every iteration if the initial conditions are changing.
        if self.change_seed_every_iter:
            self.cosmo_params.update(RANDOM_SEED=None)

    def run(self, astro_params, cosmo_params):
        """
        Actually run the 21cmFAST code.
        """
        return p21.run_coeval(
            redshift=self.redshift,
            astro_params=astro_params, flag_options=self.flag_options,
            cosmo_params=cosmo_params, user_params=self.user_params,
            perturb=self.perturb_field,
            init_box=self.initial_conditions,
            do_spin_temp=self.do_spin_temp,
            z_step_factor=self.z_step_factor,
            regenerate=self.regenerate or self.change_seed_every_iter,
            write=self.io['cache_ionize'],
            direc=self.io['cache_dir'],
            match_seed=True
        )


class CoreLightConeModule(CoreCoevalModule):
    """
    Core module for evaluating lightcone simulations.

    See :class:`~CoreCoevalModule` for info on all parameters, which are identical to this class, with the exception
    of `redshift`, which in this case must be a scalar.

    This module will add the following quantities to the context:

    1. ``lightcone``: a :class:`~py21cmmc._21cmfast.wrapper.LightCone` instance.
    """
    def __init__(self, max_redshift, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.max_redshift= max_redshift

    def setup(self):
        super().setup()

        # Un-list redshift and perturb
        self.redshift = self.redshift[0]
        if self.perturb_field is not None:
            self.perturb_field = self.perturb_field[0]

    def __call__(self, ctx):
        # Update parameters
        self._update_params(ctx.getParams())

        # Call C-code
        lightcone = self.run(self.astro_params, self.cosmo_params)

        ctx.add('lightcone', lightcone)

    def run(self, astro_params, cosmo_params):
        """
        Actually run the 21cmFAST code.
        """
        return p21.run_lightcone(
            redshift=self.redshift,
            max_redshift=self.max_redshift,
            astro_params=astro_params, flag_options=self.flag_options,
            cosmo_params=cosmo_params, user_params=self.user_params,
            perturb=self.perturb_field,
            init_box=self.initial_conditions,
            do_spin_temp=self.do_spin_temp,
            z_step_factor=self.z_step_factor,
            regenerate=self.regenerate,
            write=self.io['cache_ionize'],
            direc=self.io['cache_dir'],
            match_seed=True
        )

import pytest

import h5py
import numpy as np

from py21cmfast import (
    BrightnessTemp,
    Coeval,
    InitialConditions,
    LightCone,
    TsBox,
    global_params,
    run_coeval,
    run_lightcone,
)
from py21cmfast.lightcones import AngularLightconer, RectilinearLightconer


@pytest.fixture(scope="module")
def coeval(ic):
    return run_coeval(
        redshift=25.0, init_box=ic, write=True, flag_options={"USE_TS_FLUCT": True}
    )


@pytest.fixture(scope="module")
def lightcone(ic):
    lcn = RectilinearLightconer.with_equal_cdist_slices(
        min_redshift=25.0,
        max_redshift=35.0,
        resolution=ic.user_params.cell_size,
    )

    return run_lightcone(
        lightconer=lcn,
        init_box=ic,
        write=True,
        flag_options={"USE_TS_FLUCT": True},
    )


@pytest.fixture(scope="module")
def ang_lightcone(ic, lc):
    lcn = AngularLightconer.like_rectilinear(
        match_at_z=lc.lightcone_redshifts.min(),
        max_redshift=lc.lightcone_redshifts.max(),
        user_params=ic.user_params,
        get_los_velocity=True,
    )

    return run_lightcone(
        lightconer=lcn,
        init_box=ic,
        write=True,
        flag_options={"APPLY_RSDS": False},
    )


def test_lightcone_roundtrip(test_direc, lc):
    fname = lc.save(direc=test_direc)
    lc2 = LightCone.read(fname)

    assert lc == lc2
    assert lc.get_unique_filename() == lc2.get_unique_filename()
    assert np.all(np.isclose(lc.brightness_temp, lc2.brightness_temp))


def test_lightcone_io_abspath(lc, test_direc):
    lc.save(test_direc / "abs_path_lightcone.h5")
    assert (test_direc / "abs_path_lightcone.h5").exists()


def test_coeval_roundtrip(test_direc, coeval):
    fname = coeval.save(direc=test_direc)
    coeval2 = Coeval.read(fname)

    assert coeval == coeval2
    assert coeval.get_unique_filename() == coeval2.get_unique_filename()
    assert np.all(
        np.isclose(
            coeval.brightness_temp_struct.brightness_temp,
            coeval2.brightness_temp_struct.brightness_temp,
        )
    )


def test_coeval_cache(coeval):
    assert coeval.cache_files is not None
    out = coeval.get_cached_data(kind="brightness_temp", redshift=25.1, load_data=True)

    assert isinstance(out, BrightnessTemp)
    assert np.all(out.brightness_temp == coeval.brightness_temp)

    out = coeval.get_cached_data(
        kind="spin_temp", redshift=global_params.Z_HEAT_MAX * 1.01, load_data=True
    )

    assert isinstance(out, TsBox)
    assert not np.all(out.Ts_box == coeval.Ts_box)

    with pytest.raises(ValueError):
        coeval.get_cached_data(kind="bad", redshift=100.0)


def test_gather(coeval, test_direc):
    fname = coeval.gather(
        fname="tmpfile_test_gather.h5",
        kinds=("perturb_field", "init"),
        direc=str(test_direc),
    )

    with h5py.File(fname, "r") as fl:
        assert "cache" in fl
        assert "perturb_field" in fl["cache"]
        assert "z25.00" in fl["cache"]["perturb_field"]
        assert "density" in fl["cache"]["perturb_field"]["z25.00"]
        assert (
            fl["cache"]["perturb_field"]["z25.00"]["density"].shape
            == (coeval.user_params.HII_DIM,) * 3
        )

        assert "z0.00" in fl["cache"]["init"]


def test_lightcone_cache(lightcone):
    assert lightcone.cache_files is not None
    out = lightcone.get_cached_data(
        kind="brightness_temp", redshift=25.1, load_data=True
    )

    assert isinstance(out, BrightnessTemp)

    out = lightcone.get_cached_data(
        kind="brightness_temp", redshift=global_params.Z_HEAT_MAX * 1.01, load_data=True
    )

    assert isinstance(out, BrightnessTemp)
    assert out.redshift != lightcone.redshift

    with pytest.raises(ValueError):
        lightcone.get_cached_data(kind="bad", redshift=100.0)

    print(lightcone.cache_files)
    lightcone.gather(fname="tmp_lightcone_gather.h5", clean=["brightness_temp"])

    with pytest.raises(IOError):
        lightcone.get_cached_data(kind="brightness_temp", redshift=25.1)


def test_ang_lightcone(lc, ang_lightcone):
    # we test that the fields are "highly correlated",
    # and moreso in the one corner where the lightcones
    # should be almost exactly the same, and less so in the other
    # corners, and also less so at the highest redshifts.
    rbt = lc.brightness_temp
    abt = ang_lightcone.brightness_temp.reshape(rbt.shape)

    fullcorr0 = np.corrcoef(rbt[:, :, 0].flatten(), abt[:, :, 0].flatten())
    fullcorrz = np.corrcoef(rbt[:, :, -1].flatten(), abt[:, :, -1].flatten())

    print("correlation at low z: ", fullcorr0)
    print("correlation at highz: ", fullcorrz)
    assert fullcorr0[0, 1] > fullcorrz[0, 1]  # 0,0 and 1,1 are autocorrs.
    assert fullcorr0[0, 1] > 0.5

    # check corners
    n = rbt.shape[0]
    topcorner = np.corrcoef(
        rbt[: n // 2, : n // 2, 0].flatten(), abt[: n // 2, : n // 2, 0].flatten()
    )
    bottomcorner = np.corrcoef(
        rbt[n // 2 :, n // 2 :, 0].flatten(), abt[n // 2 :, n // 2 :, 0].flatten()
    )
    assert topcorner[0, 1] > bottomcorner[0, 1]


def test_write_to_group(ic, test_direc):
    ic.save(test_direc / "a_new_file.h5", h5_group="new_group")

    with h5py.File(test_direc / "a_new_file.h5", "r") as fl:
        assert "new_group" in fl
        assert "global_params" in fl["new_group"]

    ic2 = InitialConditions.from_file(
        test_direc / "a_new_file.h5", h5_group="new_group"
    )

    assert ic2 == ic

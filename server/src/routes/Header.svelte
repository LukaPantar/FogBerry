<script>

    import { fly } from "svelte/transition";
    import { theme } from "$lib/theme.svelte";
    import LightModeIcon from "$lib/icons/LightModeIcon.svelte";
    import DarkModeIcon from "$lib/icons/DarkModeIcon.svelte";
</script>
<style>
    header {
        display: flex;
        justify-content: space-between;
        align-items: center;
        flex-direction: row;
    }

    .right ul {
        display: flex;
        flex-direction: row;
        align-items: center;
        justify-content: center;
        gap: 8px;
    }

    ul {
        list-style-type: none;
        margin: 0;
        padding: 0;
        display: flex;
        flex-direction: row;
        align-items: center;
        justify-content: center;
    }

</style>

<header>
    <div>
        <ul>
            <li><a href="/" class="btn" style="font-size: 22px; font-weight: 600;"><nobr>BSO Projekt</nobr></a></li>
        </ul>
    </div>

    <div class="right">
        <ul>
            <li>
                <button class="btn" on:click={theme.toggle} aria-label="Toggle theme">
                    {#if theme.current === "dark"}
                        <div in:fly={{y: 10}}>
                            <DarkModeIcon />
                        </div>
                    {:else}
                        <div in:fly={{y: -10}}>
                            <LightModeIcon />
                        </div>
                    {/if}
                </button>
            </li>
        </ul>
    </div>

</header>
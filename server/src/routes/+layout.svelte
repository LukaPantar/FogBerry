<script lang="ts">
    import Header from "./Header.svelte";
    import Footer from "./Footer.svelte";
    import PageTransition from "./PageTransition.svelte";

    import "./app.css"

    let { children, data } = $props();
</script>

<style>
    .page {
        display: flex;
        flex-direction: column;
        max-width: 120vh;
        min-height: 100vh;
        padding-left: 12px;
        padding-right: 12px;
        margin: auto;
        gap: 2.5rem;
    }

    main {
        flex: auto;
    }
</style>

<div class="page">
    <Header/>

    <main>
        <PageTransition url={data.url}>
            {@render children?.()}
        </PageTransition>
    </main>

</div>
